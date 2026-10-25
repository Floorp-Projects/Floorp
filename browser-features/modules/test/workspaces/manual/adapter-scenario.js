// SPDX-License-Identifier: MPL-2.0
// Manual browser scenario; only the isolated-profile driver invokes this.
// deno-lint-ignore no-unused-vars
async function runScenario() {
  if (
    !Services.prefs.getBoolPref("floorp.workspaces.sync.manual-test", false)
  ) throw new Error("Dedicated test profile required");
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  const { WorkspacesSyncController, createRoot, testingStore } = ChromeUtils
    .importESModule(
      "resource://floorp-workspace-sync-validation/adapter.sys.mjs",
    );
  const { FloorpWorkspacesSync: api } = ChromeUtils.importESModule(
    "resource://floorp-workspace-sync-validation/engine.sys.mjs",
  );
  const { SessionStore } = ChromeUtils.importESModule(
    "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
  );
  const { ContextualIdentityService } = ChromeUtils.importESModule(
    "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
  );
  await api.ready();
  Services.obs.notifyObservers(null, "weave:service:start-over");
  await new Promise((resolve) => win.setTimeout(resolve, 20));
  const uuid = () =>
    Services.uuid.generateUUID().toString().replace(/[{}]/g, "");
  const assert = (x, m) => {
    if (!x) throw new Error(m);
  };
  const workspace = win.workspacesFuncs.createNoNameWorkspace(),
    otherWorkspace = win.workspacesFuncs.createNoNameWorkspace(),
    tabA = uuid(),
    tabB = uuid(),
    device = uuid();
  const identity = ContextualIdentityService.create(
    "Workspace Sync Test",
    "fingerprint",
    "blue",
  );
  const metadata = {
    name: "Adapter test",
    icon: null,
    userContextId: identity.userContextId,
    isSelected: null,
    isDefault: null,
  };
  const actualStore = JSON.parse(
    Services.prefs.getStringPref("floorp.workspaces.v4.store"),
  );
  actualStore.data = actualStore.data.map((
    [id, value],
  ) => [
    id,
    id === workspace || id === otherWorkspace
      ? { ...value, userContextId: identity.userContextId }
      : value,
  ]);
  Services.prefs.setStringPref(
    "floorp.workspaces.v4.store",
    JSON.stringify(actualStore),
  );
  await new Promise((resolve) => win.setTimeout(resolve, 20));
  testingStore.data.set(workspace, metadata);
  testingStore.data.set(otherWorkspace, { ...metadata, name: "Other" });
  const ctx = {
    getRawWorkspace: (id) => testingStore.data.get(id),
    withExplicitTabUserContext: (_id, open) => open(),
    tabManagerCtx: {
      setWorkspaceIdToAttribute: (tab, id) =>
        tab.setAttribute("floorpWorkspaceId", id),
      updateTabsVisibility: () => {},
    },
  };
  let dispose, constructionError;
  const controller = createRoot((d) => {
    dispose = d;
    try {
      return new WorkspacesSyncController(ctx);
    } catch (error) {
      constructionError = String(error) + " " + error.stack;
      return null;
    }
  });
  if (!controller) throw new Error(constructionError);
  const originalTabs = new Set(win.gBrowser.tabs);
  try {
    for (let i = 0; i < 100 && !controller.isAvailable(); i++) {
      await new Promise((r) => win.setTimeout(r, 20));
    }
    assert(
      controller.isAvailable(),
      "controller registers after SessionStore restoration",
    );
    const document = {
      version: 1,
      workspaceId: workspace,
      name: "Adapter test",
      icon: null,
      metadata: { modifiedAt: 100, modifiedBy: device },
      tabs: [tabA, tabB].map((id, position) => ({
        id,
        url: "__WORKSPACE_SYNC_TEST_ORIGIN__/" + id,
        title: id,
        pinned: false,
        position,
        deleted: false,
        modifiedAt: 100,
        modifiedBy: device,
      })),
    };
    await controller.apply(document, new Set([tabA, tabB]));
    const tabs = [...win.gBrowser.tabs].filter((tab) =>
      tab.getAttribute("floorpWorkspaceId") === workspace &&
      [tabA, tabB].includes(
        SessionStore.getCustomTabValue(tab, "floorp-workspace-sync-id"),
      )
    );
    assert(tabs.length === 2, "remote tabs created once");
    assert(
      tabs.every((tab) =>
        Number(tab.getAttribute("usercontextid")) === identity.userContextId
      ),
      "remote tabs use receiver's local workspace container",
    );
    assert(
      new Set(
        tabs.map((tab) =>
          SessionStore.getCustomTabValue(tab, "floorp-workspace-sync-id")
        ),
      ).size === 2,
      "stable tab IDs persisted through SessionStore",
    );
    let snapshot = controller.capture(workspace, new Set());
    assert(
      snapshot.tabs.length === 2 && !snapshot.orderChanged,
      "remote apply establishes physical-order baseline",
    );
    const positions = [...win.gBrowser.tabs];
    win.gBrowser.moveTabTo(tabs[1], { tabIndex: positions.indexOf(tabs[0]) });
    snapshot = controller.capture(workspace, new Set());
    assert(snapshot.orderChanged, "actual local tab reorder is detected");
    await controller.apply(document, new Set());
    assert(
      !controller.capture(workspace, new Set()).orderChanged,
      "receiving remote order does not echo a reorder",
    );
    const duplicate = win.gBrowser.addTab(
      "__WORKSPACE_SYNC_TEST_ORIGIN__/duplicate",
      {
        userContextId: identity.userContextId,
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    duplicate.setAttribute("floorpWorkspaceId", otherWorkspace);
    SessionStore.setCustomTabValue(duplicate, "floorp-workspace-sync-id", tabA);
    SessionStore.setCustomTabValue(
      duplicate,
      "floorp-workspace-sync-owner",
      workspace,
    );
    SessionStore.setCustomTabValue(
      duplicate,
      "floorp-workspace-sync-url",
      "__WORKSPACE_SYNC_TEST_ORIGIN__/duplicate",
    );
    const moved = controller.capture(otherWorkspace, new Set());
    assert(
      moved.tabs.length === 1 && moved.tabs[0].id !== tabA,
      "moving or duplicating a tab to another workspace gets a new global ID",
    );
    await new Promise((resolve) => win.setTimeout(resolve, 500));
    await api.select(workspace, true);
    const closeTarget = [...win.gBrowser.tabs].find((tab) =>
      SessionStore.getCustomTabValue(tab, "floorp-workspace-sync-id") === tabA
    );
    win.gBrowser.removeTab(closeTarget, { animate: false });
    await new Promise((resolve) => win.setTimeout(resolve, 200));
    await api.capture();
    assert(
      api.isRetired(tabA),
      "local tab close retires its UUID " + JSON.stringify({
        selected: api.isSelected(workspace),
        error: api.errorFor(workspace),
        tabs: controller.capture(workspace, new Set()),
        closed: !closeTarget.isConnected,
        custom: SessionStore.getCustomTabValue(
          closeTarget,
          "floorp-workspace-sync-id",
        ),
      }),
    );
    let closedIndex = -1;
    for (let attempt = 0; attempt < 200; attempt++) {
      closedIndex = SessionStore.getClosedTabDataForWindow(win).findIndex((
        entry,
      ) => entry.state.extData?.["floorp-workspace-sync-id"] === tabA);
      if (closedIndex >= 0) break;
      await new Promise((resolve) => win.setTimeout(resolve, 20));
    }
    assert(
      closedIndex >= 0,
      "SessionStore finished recording the specific closed tab",
    );
    const reopened = SessionStore.undoCloseTab(win, closedIndex);
    await new Promise((resolve) => win.setTimeout(resolve, 400));
    const restoredSnapshot = controller.capture(
      reopened.getAttribute("floorpWorkspaceId"),
      new Set(),
    );
    const restoredID = SessionStore.getCustomTabValue(
      reopened,
      "floorp-workspace-sync-id",
    );
    assert(
      restoredID !== tabA &&
        restoredSnapshot.tabs.some((tab) => tab.id === restoredID),
      "native Undo Close assigns a new UUID after extData restoration " +
        JSON.stringify({
          restoredID,
          tabA,
          restoredSnapshot,
          owner: reopened.getAttribute("floorpWorkspaceId"),
          explicit: controller.explicitlyOpened.has(reopened),
          after: controller.openedAfterStartup?.has(reopened),
        }),
    );
    await api.select(workspace, false);
    const menu = win.document.createXULElement("menuitem");
    menu.setAttribute("type", "checkbox");
    menu.setAttribute("autocheck", "false");
    menu.dispatchEvent(new win.Event("command", { bubbles: true }));
    assert(
      !menu.hasAttribute("checked"),
      "checkbox does not toggle ahead of asynchronous selection",
    );
    const fixtureWorkspace = (name) => {
      const id = win.workspacesFuncs.createNoNameWorkspace();
      testingStore.data.set(id, { ...metadata, name });
      return id;
    };
    const open = (id, url) => {
      const tab = win.gBrowser.addTab(url, {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      });
      tab.setAttribute("floorpWorkspaceId", id);
      return tab;
    };
    const waitForNavigation = async (tab, url) => {
      const browser = win.gBrowser.getBrowserForTab(tab);
      for (let attempt = 0; attempt < 200; attempt++) {
        if (
          browser.currentURI?.spec === url &&
          !browser.webProgress.isLoadingDocument
        ) return;
        await new Promise((resolve) => win.setTimeout(resolve, 20));
      }
      throw new Error("Regression page did not finish loading");
    };
    const navigate = async (tab, url) => {
      win.gBrowser.getBrowserForTab(tab).loadURI(Services.io.newURI(url), {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      });
      await waitForNavigation(tab, url);
    };
    const mustReject = async (promise, message) => {
      let rejected = false;
      try {
        await promise;
      } catch {
        rejected = true;
      }
      assert(rejected, message);
    };
    const incomingWorkspace = fixtureWorkspace(
      "Incoming navigation regression",
    );
    const originalURL = "__WORKSPACE_SYNC_TEST_ORIGIN__/incoming-original";
    const incomingTab = open(incomingWorkspace, originalURL);
    await waitForNavigation(incomingTab, originalURL);
    const originalSnapshot = controller.capture(incomingWorkspace, new Set());
    const incomingID = originalSnapshot.tabs[0].id;
    const incomingDocument = (url) => ({
      version: 1,
      workspaceId: incomingWorkspace,
      name: "Incoming navigation regression",
      icon: null,
      metadata: { modifiedAt: 300, modifiedBy: device },
      tabs: [{
        id: incomingID,
        url,
        title: "Incoming intended title",
        pinned: false,
        position: 0,
        deleted: false,
        modifiedAt: 300,
        modifiedBy: device,
      }],
    });
    const slowURL = () =>
      "__WORKSPACE_SYNC_TEST_ORIGIN__/sync-slow?delay=1500&token=" + uuid();
    const remoteURL = slowURL();
    const receiving = controller.apply(incomingDocument(remoteURL), new Set());
    let received = false;
    receiving.then(() => received = true, () => received = true);
    await new Promise((resolve) => win.setTimeout(resolve, 50));
    const beforeCommit = controller.capture(incomingWorkspace, new Set());
    assert(
      !received &&
        win.gBrowser.getBrowserForTab(incomingTab).currentURI.spec ===
          originalURL &&
        beforeCommit.tabs[0].url === remoteURL &&
        beforeCommit.tabs[0].title === "Incoming intended title",
      "an existing slow remote load retains its URL and title intent before native commit",
    );
    const explicitDuringReceive = open(
      otherWorkspace,
      "__WORKSPACE_SYNC_TEST_ORIGIN__/explicit-open-during-receive",
    );
    assert(
      controller.explicitlyOpened.has(explicitDuringReceive) &&
        controller.openedAfterStartup.has(explicitDuringReceive),
      "real user TabOpen remains explicit while the remote navigation waits",
    );
    await receiving;
    await waitForNavigation(incomingTab, remoteURL);
    win.gBrowser.getBrowserForTab(incomingTab).goBack();
    await waitForNavigation(incomingTab, originalURL);
    const afterBack = controller.capture(incomingWorkspace, new Set());
    assert(
      afterBack.tabs[0].url === originalURL &&
        afterBack.tabs[0].id === incomingID,
      "native Back to the exact original URL is captured with its stable ID",
    );
    const superseded = controller.apply(
      incomingDocument(slowURL()),
      new Set(),
    );
    superseded.catch(() => {});
    await new Promise((resolve) => win.setTimeout(resolve, 50));
    await navigate(incomingTab, originalURL);
    await superseded;
    assert(
      controller.capture(incomingWorkspace, new Set()).tabs[0].url ===
        originalURL,
      "explicit navigation to the exact pre-apply URL supersedes remote intent successfully",
    );
    const previousURL =
      "__WORKSPACE_SYNC_TEST_ORIGIN__/incoming-history-previous";
    await navigate(incomingTab, previousURL);
    await navigate(incomingTab, originalURL);
    const backDuringReceive = controller.apply(
      incomingDocument(slowURL()),
      new Set(),
    );
    backDuringReceive.catch(() => {});
    await new Promise((resolve) => win.setTimeout(resolve, 50));
    win.gBrowser.getBrowserForTab(incomingTab).goBack();
    await waitForNavigation(incomingTab, previousURL);
    await backDuringReceive;
    assert(
      controller.capture(incomingWorkspace, new Set()).tabs[0].url ===
        previousURL,
      "native history Back during a slow incoming load preserves the actual previous URL",
    );
    await navigate(incomingTab, originalURL);
    await mustReject(
      controller.apply(
        incomingDocument(
          "__WORKSPACE_SYNC_TEST_ORIGIN__/sync-no-content",
        ),
        new Set(),
      ),
      "a successful HTTP response without a document commit rejects the incoming apply",
    );
    const navigationAbort = new AbortController();
    const aborted = controller.apply(
      incomingDocument(slowURL()),
      new Set(),
      navigationAbort.signal,
    );
    const abortedResult = mustReject(aborted, "aborted incoming load rejects");
    await new Promise((resolve) => win.setTimeout(resolve, 50));
    navigationAbort.abort();
    await abortedResult;
    await new Promise((resolve) => win.setTimeout(resolve, 1600));
    assert(
      controller.incomingNavigations.size === 0 &&
        win.gBrowser.getBrowserForTab(incomingTab).currentURI.spec ===
          originalURL,
      "abort clears the transient intent and stops its owned load before later commit",
    );
    const safeWorkspace = fixtureWorkspace("Internal navigation regression");
    const safeURL = "__WORKSPACE_SYNC_TEST_ORIGIN__/safe";
    const safeTab = open(safeWorkspace, safeURL);
    await navigate(safeTab, safeURL);
    safeTab.setAttribute("label", "Public title");
    await api.select(safeWorkspace, true);
    const safeID = SessionStore.getCustomTabValue(
      safeTab,
      "floorp-workspace-sync-id",
    );
    const safeRecord = api.getSyncedTab(safeWorkspace, safeID);
    assert(
      safeRecord && safeRecord.url === safeURL,
      "public record captured before internal navigation",
    );
    for (const internalURL of ["about:blank", "about:preferences#privacy"]) {
      await navigate(safeTab, internalURL);
      safeTab.setAttribute("label", "PRIVATE INTERNAL TITLE");
      win.gBrowser.pinTab(safeTab);
      await api.capture();
      const retained = api.getSyncedTab(safeWorkspace, safeID);
      assert(
        !api.isRetired(safeID) && retained?.url === safeURL &&
          retained.title === safeRecord.title &&
          retained.pinned === safeRecord.pinned,
        "internal navigation retains the public record without leaking its title or retiring its ID",
      );
      await controller.apply({
        version: 1,
        workspaceId: safeWorkspace,
        name: "Internal navigation regression",
        icon: null,
        metadata: { modifiedAt: 200, modifiedBy: device },
        tabs: [{
          ...safeRecord,
          deleted: false,
          position: 0,
          modifiedAt: 200,
          modifiedBy: device,
        }],
      }, new Set());
      assert(
        win.gBrowser.getBrowserForTab(safeTab).currentURI.spec ===
            internalURL &&
          safeTab.hasAttribute("pinned"),
        "incoming apply preserves a local internal page and pin state",
      );
    }
    const resumedURL = "__WORKSPACE_SYNC_TEST_ORIGIN__/resumed";
    await navigate(safeTab, resumedURL);
    safeTab.setAttribute("label", "Public resumed title");
    await api.capture();
    assert(
      SessionStore.getCustomTabValue(safeTab, "floorp-workspace-sync-id") ===
          safeID &&
        api.getSyncedTab(safeWorkspace, safeID)?.url === resumedURL,
      "returning to a public page resumes Sync with the same stable ID",
    );

    const moveSource = fixtureWorkspace("Move source regression");
    const moveDestination = fixtureWorkspace("Move destination regression");
    const moving = open(moveSource, "__WORKSPACE_SYNC_TEST_ORIGIN__/moving");
    await navigate(moving, "__WORKSPACE_SYNC_TEST_ORIGIN__/moving");
    await api.select(moveSource, true);
    await api.select(moveDestination, true);
    await api.capture();
    const oldMoveID = SessionStore.getCustomTabValue(
      moving,
      "floorp-workspace-sync-id",
    );
    moving.setAttribute("floorpWorkspaceId", moveDestination);
    await api.capture();
    const newMoveID = SessionStore.getCustomTabValue(
      moving,
      "floorp-workspace-sync-id",
    );
    assert(
      api.isRetired(oldMoveID) && newMoveID !== oldMoveID &&
        api.getSyncedTab(moveDestination, newMoveID)?.url ===
          "__WORKSPACE_SYNC_TEST_ORIGIN__/moving",
      "source-first capture gives a moved physical tab a new live destination ID",
    );

    const privateDeletion = fixtureWorkspace(
      "Private metadata deletion regression",
    );
    await api.select(privateDeletion, true);
    const privateController = Object.create(WorkspacesSyncController.prototype);
    privateController.api = api;
    privateController.privateWindow = true;
    await privateController.deleteWorkspace(privateDeletion);
    assert(
      api.isRetired(privateDeletion) && !api.isSelected(privateDeletion),
      "profile-wide deletion from a private controller sends only the workspace tombstone",
    );
    return {
      passed: true,
      cases: [
        "native tab creation and local container",
        "SessionStore stable IDs",
        "local reorder detection and receive baseline",
        "global ID regeneration on workspace move",
        "checkbox state ownership",
        "native Undo Close after SessionStore restoration",
        "internal navigation preserves public records and private titles",
        "incoming apply preserves local internal pages and pin state",
        "return to a public page retains its stable ID",
        "source-first workspace move replaces a retired ID",
        "private-window profile-wide metadata deletion",
        "slow existing-tab remote navigation preserves URL and title before commit",
        "native Back and explicit same-original-URL navigation clear remote intent",
        "HTTP 204 without commit rejects instead of claiming applied",
        "account abort stops pending navigation and releases its intent",
        "native Back during slow incoming navigation preserves history intent",
        "real user TabOpen remains explicit during a pending remote apply",
      ],
    };
  } finally {
    try {
      dispose();
    } catch (error) {
      console.error(error);
    }
    for (const tab of [...win.gBrowser.tabs]) {
      if (!originalTabs.has(tab)) {
        win.gBrowser.removeTab(tab, { animate: false });
      }
    }
    ContextualIdentityService.remove(identity.userContextId);
  }
}
