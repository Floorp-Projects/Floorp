// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function waitFor<T>(
  read: () => T | false | undefined | Promise<T | false | undefined>,
  description: string,
): Promise<T> {
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for ${description}`);
}

type FixtureTab = {
  linkedBrowser: {
    contentTitle: string;
    currentURI: { spec: string };
    browsingContext: Record<string, unknown>;
  };
};
type FixtureWindow = {
  wrappedJSObject: { __floorpRuntimeFixtureURL: string };
  gBrowser: {
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): FixtureTab;
    selectedTab: FixtureTab;
    removeTab(tab: FixtureTab): void;
  };
};
async function testNativeDebugger(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const { ChromeDebuggerSession } = ChromeUtils.importESModule(
    "resource://gre/modules/ChromeDebugger.sys.mjs",
  );
  const id = "ab".repeat(16), geckoId = `${id}@chromium-extension`;
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-debugger-${Services.uuid.generateUUID()}.xpi`,
  );
  const encode = (value: string) => new TextEncoder().encode(value);
  await IOUtils.write(
    path,
    zipSync({
      "manifest.json": encode(JSON.stringify({
        manifest_version: 3,
        name: "Debugger native fixture",
        version: "1.0",
        permissions: ["debugger"],
        background: { scripts: ["background.js"] },
        browser_specific_settings: {
          gecko: {
            id: geckoId,
            floorp_chrome_extension: { schema_version: 1, original_id: id },
          },
        },
      })),
      "background.js": encode(`
      const state={ready:typeof chrome.debugger?.attach==="function",events:[],detaches:[]};
      const publish=()=>document.title=JSON.stringify(state);
      chrome.debugger.onEvent.addListener((source,method,params)=>{state.events.push({source,method,params});publish()});
      chrome.debugger.onDetach.addListener((source,reason)=>{state.detaches.push({source,reason});publish()});
      window.addEventListener("message",async({data})=>{
        if(!data?.floorpDebuggerFixture)return;
        try{state.value=await browser.debugger[data.method](...data.args);delete state.error}
        catch(error){state.error=String(error)}
        state.sequence=data.sequence;publish();
      });publish();
    `),
    }),
  );
  const uploadPath = PathUtils.join(
    PathUtils.tempDir,
    `floorp-debugger-upload-${Services.uuid.generateUUID()}.txt`,
  );
  await IOUtils.writeUTF8(uploadPath, "native debugger file content");
  let addon;
  let tab: FixtureTab | null = null;
  let session: InstanceType<typeof ChromeDebuggerSession> | null = null;
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as FixtureWindow;
  const events: { method: string; params: Record<string, unknown> }[] = [];
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(geckoId),
      "extension startup",
    );
    await waitFor(
      () =>
        extension.backgroundContext?.xulBrowser.contentTitle.includes(
          '"ready":true',
        ),
      "public debugger binding",
    );
    const html =
      '<title>Debugger target</title><style>#item{padding:7px;border:3px solid;margin:5px}</style><div id="item" data-old="before">hello</div><input id="editor" value="before"><input type="file" id="upload" onchange="globalThis.uploadChanges=(globalThis.uploadChanges||0)+1"><button id="button" onclick="globalThis.clicked=(globalThis.clicked||0)+1">Click</button>';
    tab = win.gBrowser.addTab("data:text/html," + encodeURIComponent(html), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    win.gBrowser.selectedTab = tab;
    await waitFor(
      () => tab!.linkedBrowser.contentTitle === "Debugger target",
      "target load",
    );
    const tabId = ExtensionParent.apiManager.global.tabTracker.getId(tab);
    let sequence = 0;
    const publicCommand = async (method: string, ...args: unknown[]) => {
      const current = ++sequence;
      extension.backgroundContext.xulBrowser.messageManager.loadFrameScript(
        `data:application/javascript,${
          encodeURIComponent(
            `content.postMessage(${
              JSON.stringify({
                floorpDebuggerFixture: true,
                sequence: current,
                method,
                args,
              })
            },'*')`,
          )
        }`,
        false,
      );
      const result = await waitFor(() => {
        try {
          const state = JSON.parse(
            extension.backgroundContext.xulBrowser.contentTitle,
          );
          return state.sequence === current ? state : false;
        } catch {
          return false;
        }
      }, `public ${method}`);
      if (result.error) throw new Error(`public ${method}: ${result.error}`);
      return result.value;
    };
    session = new ChromeDebuggerSession(
      extension,
      tab,
      tabId,
      (_source: unknown, method: string, params: Record<string, unknown>) =>
        events.push({ method, params }),
      () => {},
    );
    await session.open();
    const command = async (
      method: string,
      params: Record<string, unknown> = {},
    ) => {
      try {
        return await session!.command({ tabId }, method, params);
      } catch (error) {
        throw new Error(`${method}: ${String(error)} ${JSON.stringify(error)}`);
      }
    };
    assertEquals(
      (await command("Runtime.evaluate", { expression: "1+2" })).result.value,
      3,
      "native console evaluation",
    );
    const otherOwner = new ChromeDebuggerSession(
      extension,
      tab,
      tabId,
      () => {},
      () => {},
    );
    let owned = false;
    try {
      await otherOwner.open();
    } catch (error) {
      owned = String(error).includes("already attached");
    }
    assert(owned, "native target admits exactly one debugger owner");
    await command("Runtime.enable");
    assert(
      events.some((event) =>
        event.method === "Runtime.executionContextCreated"
      ),
      "native execution contexts emit",
    );
    const object =
      (await command("Runtime.evaluate", { expression: "({count:2})" })).result;
    assert(object.objectId, "native remote object retained");
    const called = await command("Runtime.callFunctionOn", {
      objectId: object.objectId,
      functionDeclaration: "function(n){this.count+=n;return this.count}",
      arguments: [{ value: 3 }],
      returnByValue: true,
    });
    assertEquals(
      called.result.value,
      5,
      "native remote function mutates receiver",
    );
    const props = await command("Runtime.getProperties", {
      objectId: object.objectId,
      ownProperties: true,
    });
    assert(
      props.result.some((
        property: { name: string; value?: { value: unknown } },
      ) => property.name === "count" && property.value?.value === 5),
      "native properties reflect mutation",
    );
    const promise = await command("Runtime.evaluate", {
      expression: "Promise.resolve({answer:7})",
      awaitPromise: true,
      returnByValue: true,
    });
    assertEquals(promise.result.value.answer, 7, "native promise completion");
    await command("DOM.enable");
    const { root } = await command("DOM.getDocument", { depth: 1 });
    const { nodeId } = await command("DOM.querySelector", {
      nodeId: root.nodeId,
      selector: "#item",
    });
    assert(nodeId > 0, "native inspector query");
    await command("DOM.setAttributeValue", {
      nodeId,
      name: "data-old",
      value: "after",
    });
    assert(
      (await command("DOM.getOuterHTML", { nodeId })).outerHTML.includes(
        'data-old="after"',
      ),
      "native inspector attribute mutation",
    );
    const { model } = await command("DOM.getBoxModel", { nodeId });
    assert(
      model.content[0] !== model.border[0],
      "actual content and border quads differ",
    );
    const resolved = (await command("DOM.resolveNode", { nodeId })).object;
    assert(resolved.objectId, "native node resolves to remote object");
    const { nodeId: inputId } = await command("DOM.querySelector", {
      nodeId: root.nodeId,
      selector: "#editor",
    });
    await command("DOM.focus", { nodeId: inputId });
    await command("Input.insertText", { text: " inserted" });
    await waitFor(
      async () =>
        (await command("Runtime.evaluate", {
          expression: "document.querySelector('#editor').value",
        })).result.value.includes("inserted"),
      "native text input changes editor",
    );
    await command("Input.dispatchKeyEvent", {
      type: "rawKeyDown",
      key: "a",
      code: "KeyA",
      windowsVirtualKeyCode: 65,
      nativeVirtualKeyCode: 65,
      modifiers: 2,
    });
    await command("Input.dispatchKeyEvent", {
      type: "keyUp",
      key: "a",
      code: "KeyA",
      windowsVirtualKeyCode: 65,
      modifiers: 2,
    });
    await command("Input.dispatchKeyEvent", {
      type: "keyUp",
      key: "Control",
      code: "ControlLeft",
      windowsVirtualKeyCode: 17,
      modifiers: 0,
    });
    await command("Input.insertText", { text: "native keyboard replacement" });
    await waitFor(
      async () =>
        (await command("Runtime.evaluate", {
          expression: "document.querySelector('#editor').value",
        })).result.value === "native keyboard replacement",
      "native modified key input selects actual text",
    );
    const button = (await command("Runtime.evaluate", {
      expression:
        "(()=>{const r=document.querySelector('#button').getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2}})()",
      returnByValue: true,
    })).result.value;
    await command("Input.dispatchMouseEvent", {
      type: "mousePressed",
      button: "left",
      x: button.x,
      y: button.y,
      clickCount: 1,
    });
    await command("Input.dispatchMouseEvent", {
      type: "mouseReleased",
      button: "left",
      x: button.x,
      y: button.y,
      clickCount: 1,
    });
    await waitFor(
      async () =>
        (await command("Runtime.evaluate", {
          expression: "globalThis.clicked",
        })).result.value === 1,
      "native mouse input activates actual button",
    );
    const { nodeId: fileId } = await command("DOM.querySelector", {
      nodeId: root.nodeId,
      selector: "#upload",
    });
    await command("DOM.setFileInputFiles", {
      nodeId: fileId,
      files: [uploadPath],
    });
    assertEquals(
      (await command("Runtime.evaluate", {
        expression: "document.querySelector('#upload').files[0].size",
      })).result.value,
      28,
      "native file input receives actual file bytes",
    );
    assertEquals(
      (await command("Runtime.evaluate", {
        expression: "globalThis.uploadChanges",
      })).result.value,
      1,
      "native file input dispatches change",
    );
    const fileObject = (await command("Runtime.evaluate", {
      expression: "document.querySelector('#upload')",
    })).result;
    await command("DOM.setFileInputFiles", {
      objectId: fileObject.objectId,
      files: [],
    });
    assertEquals(
      (await command("Runtime.evaluate", {
        expression: "document.querySelector('#upload').files.length",
      })).result.value,
      0,
      "native objectId file-input routing",
    );
    await command("Page.enable");
    const dialogResult = command("Runtime.evaluate", {
      expression: "globalThis.promptAnswer=prompt('native prompt','before')",
    });
    await waitFor(
      () =>
        events.find((event) => event.method === "Page.javascriptDialogOpening"),
      "native dialog event",
    );
    await command("Page.handleJavaScriptDialog", {
      accept: true,
      promptText: "native accepted",
    });
    await dialogResult;
    assertEquals(
      (await command("Runtime.evaluate", {
        expression: "globalThis.promptAnswer",
      })).result.value,
      "native accepted",
      "native dialog handler changes real prompt result",
    );
    await waitFor(
      () =>
        events.find((event) => event.method === "Page.javascriptDialogClosed"),
      "native dialog close event",
    );
    const metrics = await command("Page.getLayoutMetrics");
    assert(metrics.cssLayoutViewport.clientWidth > 0, "native page metrics");
    const shot = await command("Page.captureScreenshot", { format: "png" });
    assert(shot.data.startsWith("iVBOR"), "real PNG screenshot");
    const targets = await command("Target.getTargets");
    assert(targets.targetInfos.length >= 1, "actual native targets exposed");
    // Navigate into a real HTTP origin, forcing a native content-process transition.
    const oldNode = nodeId;
    const fixtureURL = win.wrappedJSObject.__floorpRuntimeFixtureURL;
    await command("Page.navigate", { url: fixtureURL + "worker-target.html" });
    await waitFor(
      () => tab!.linkedBrowser.currentURI.spec.startsWith(fixtureURL),
      "HTTP target navigation",
    );
    assertEquals(
      (await command("Runtime.evaluate", { expression: "location.origin" }))
        .result.value,
      new URL(fixtureURL).origin,
      "native debugger survives process switch",
    );
    let staleRejected = false;
    try {
      await command("DOM.getOuterHTML", { nodeId: oldNode });
    } catch {
      staleRejected = true;
    }
    assert(staleRejected, "navigation invalidates native node handles");
    await command("Network.enable");
    await command("Network.setUserAgentOverride", {
      userAgent: "Floorp-Debugger-Native-Fixture",
    });
    await command("Network.setCacheDisabled", { cacheDisabled: true });
    const networkReply = await command("Runtime.evaluate", {
      expression:
        `fetch('/debugger-echo',{method:'POST',body:'native-post-body',headers:{'X-Debugger-Request':'actual'}}).then(r=>r.json())`,
      awaitPromise: true,
      returnByValue: true,
    });
    assertEquals(
      networkReply.result.value.headers["User-Agent"],
      "Floorp-Debugger-Native-Fixture",
      "native network user agent configuration changes actual request",
    );
    assertEquals(
      networkReply.result.value.body,
      "native-post-body",
      "actual POST reaches HTTP server",
    );
    const requestEvent = await waitFor(
      () =>
        events.find((event) =>
          event.method === "Network.requestWillBeSent" &&
          String((event.params.request as { url: string })?.url).includes(
            "debugger-echo",
          )
        ),
      "native network request event",
    );
    const requestId = requestEvent.params.requestId;
    await waitFor(
      () =>
        events.find((event) =>
          event.method === "Network.loadingFinished" &&
          event.params.requestId === requestId
        ),
      "native network completion",
    );
    const body = await command("Network.getResponseBody", { requestId });
    assertEquals(
      JSON.parse(body.body).body,
      "native-post-body",
      "native Network actor retains actual response body",
    );
    const post = await command("Network.getRequestPostData", { requestId });
    assertEquals(
      post.postData,
      "native-post-body",
      "native Network actor retains actual request body",
    );
    let rejected = false;
    try {
      await command("Runtime.thisIsNotImplemented");
    } catch (error) {
      rejected = String(error).includes("Unsupported CDP command");
    }
    assert(rejected, "unimplemented command rejects explicitly");
    const finalObject =
      (await command("Runtime.evaluate", { expression: "({final:true})" }))
        .result;
    await command("Runtime.releaseObject", { objectId: finalObject.objectId });
    rejected = false;
    try {
      await command("Runtime.getProperties", {
        objectId: finalObject.objectId,
      });
    } catch {
      rejected = true;
    }
    assert(rejected, "released object cannot be reused");
    await session.close(null);
    session = null;
    await publicCommand("attach", { tabId }, "1.3");
    const publicTargets = await publicCommand("getTargets");
    assert(
      publicTargets.some((target: { tabId: number; attached: boolean }) =>
        target.tabId === tabId && target.attached
      ),
      "public native API reports owned attachment",
    );
    assertEquals(
      (await publicCommand("sendCommand", { tabId }, "Runtime.evaluate", {
        expression: "20+22",
      })).result.value,
      42,
      "real extension IPC evaluates through DevTools adapter",
    );
    await publicCommand("sendCommand", { tabId }, "Runtime.enable");
    await waitFor(
      () =>
        JSON.parse(extension.backgroundContext.xulBrowser.contentTitle).events
          .some((event: { method: string }) =>
            event.method === "Runtime.executionContextCreated"
          ),
      "public native event delivery",
    );
    rejected = false;
    try {
      await publicCommand("attach", { tabId }, "1.3");
    } catch {
      rejected = true;
    }
    assert(rejected, "public duplicate attachment is rejected");
    await publicCommand("detach", { tabId });
    rejected = false;
    try {
      await publicCommand("sendCommand", { tabId }, "Runtime.evaluate", {
        expression: "1",
      });
    } catch {
      rejected = true;
    }
    assert(rejected, "detached extension cannot issue commands");
    await publicCommand("attach", { tabId }, "1.3");
    await extension.terminateBackground();
    win.gBrowser.removeTab(tab);
    tab = null;
    await waitFor(
      () => {
        try {
          return JSON.parse(
            extension.backgroundContext?.xulBrowser.contentTitle || "{}",
          ).detaches
            ?.some((event: { reason: string }) =>
              event.reason === "target_closed"
            );
        } catch {
          return false;
        }
      },
      "native tab-close detach event",
    );
  } finally {
    await session?.close(null);
    if (tab) {
      win.gBrowser.removeTab(tab);
    }
    await addon?.uninstall();
    await IOUtils.remove(path);
    await IOUtils.remove(uploadPath);
  }
}
async function testOrdinaryFirefoxIsolation(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const id = "floorp-debugger-ordinary@example.invalid";
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-debugger-ordinary-${Services.uuid.generateUUID()}.xpi`,
  );
  const encode = (value: string) => new TextEncoder().encode(value);
  await IOUtils.write(
    path,
    zipSync({
      "manifest.json": encode(JSON.stringify({
        manifest_version: 2,
        name: "Ordinary Firefox debugger isolation",
        version: "1.0",
        permissions: ["debugger"],
        background: { scripts: ["background.js"], persistent: false },
        browser_specific_settings: { gecko: { id } },
      })),
      "background.js": encode(
        "document.title='debugger:'+typeof chrome.debugger;",
      ),
    }),
  );
  let addon;
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "ordinary Firefox extension startup",
    );
    await waitFor(
      () =>
        extension.backgroundContext?.xulBrowser.contentTitle ===
          "debugger:undefined",
      "ordinary Firefox debugger namespace isolation",
    );
  } finally {
    await addon?.uninstall();
    await IOUtils.remove(path);
  }
}
export async function runAllTests(): Promise<void> {
  await runTests("Chrome debugger native DevTools", [{
    name: "ordinary Firefox namespace isolation",
    fn: testOrdinaryFirefoxIsolation,
  }, {
    name: "actual RDP evaluation DOM input screenshot and ownership",
    fn: testNativeDebugger,
  }]);
}
