// SPDX-License-Identifier: MPL-2.0

import Workspaces from "../../workspaces/index.ts";
import { workspacesDataStore } from "../../workspaces/data/data.ts";
import {
  WORKSPACE_DATA_PREF_NAME,
  WORKSPACE_LAST_SHOW_ID,
  WORKSPACES_CHANGED_OBSERVER_TOPIC,
} from "../../workspaces/utils/workspaces-static-names.ts";
import {
  settle,
  waitFor,
  withWindows,
} from "../../workspaces/test/workspace-last-tab-test-utils.ts";
import {
  KEYBOARD_SHORTCUT_CONFIG_PREF,
  KEYBOARD_SHORTCUT_ENABLED_PREF,
} from "../../keyboard-shortcut/config.ts";
import { NATIVE_GESTURE_PREF, SIDEBAR_GESTURE_PREF } from "../policy.ts";
import { assert, assertEquals } from "../../../test/utils/test_harness.ts";
import type {
  NativeInputDirection,
  NativeInputEvidence,
  NativeInputKey,
  NativeInputReply,
  NativeInputSavedPref,
  NativeInputSnapshot,
  NativeInputStep,
  NativeInputSubprocess,
  NativeInputWindow,
} from "./windows-native-input-types.ts";

const EVIDENCE_FILE = "workspace-windows-input-evidence.json";
const { Subprocess } = ChromeUtils.importESModule(
  "resource://gre/modules/Subprocess.sys.mjs",
) as { Subprocess: NativeInputSubprocess };
const { setTimeout: nativeSetTimeout, clearTimeout: nativeClearTimeout } =
  ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");

function fixturePaths(): { repo: nsIFile; helper: nsIFile; evidence: string } {
  const profile = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let current: nsIFile | null = profile;
  for (const component of ["test", "profile", "_dist"]) {
    assert(
      current?.leafName === component && current.isDirectory() &&
        !current.isSymlink(),
      "native input requires the repository's real _dist/profile/test fixture",
    );
    current = current.parent;
  }
  assert(current, "repository root exists above the test profile");
  const repo = current;
  const lock = repo.clone();
  lock.append("floorp-runtime.lock.json");
  assert(lock.isFile() && !lock.isSymlink(), "repository Runtime lock exists");
  const helper = repo.clone();
  helper.append("tools");
  assert(
    helper.isDirectory() && !helper.isSymlink(),
    "tools is a real directory",
  );
  helper.append("send_windows_workspace_input.ps1");
  assert(
    helper.isFile() && !helper.isSymlink(),
    "checked-in input helper exists",
  );
  const evidence = profile.clone();
  evidence.append(EVIDENCE_FILE);
  return { repo, helper, evidence: evidence.path };
}

function windowHandle(win: NativeInputWindow): string {
  const shell = win.docShell;
  assert(shell, "browser window has a live docShell");
  const owner = shell.treeOwner;
  assert(owner.QueryInterface, "browser tree owner supports native interfaces");
  const handle = owner.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIBaseWindow).nativeHandle;
  assert(
    /^0x[0-9a-f]+$/i.test(handle) && BigInt(handle) !== 0n,
    "real HWND exists",
  );
  return handle;
}

async function sendNativeInput(
  win: NativeInputWindow,
  direction: NativeInputDirection,
  repeatCount: number,
): Promise<NativeInputReply> {
  const paths = fixturePaths();
  const powershell = Services.dirsvc.get("SysD", Ci.nsIFile);
  for (const component of ["WindowsPowerShell", "v1.0", "powershell.exe"]) {
    powershell.append(component);
  }
  assert(powershell.isFile(), "Windows PowerShell is available");
  const handle = windowHandle(win);
  const process = await Subprocess.call({
    command: powershell.path,
    arguments: [
      "-NoProfile",
      "-NonInteractive",
      "-ExecutionPolicy",
      "Bypass",
      "-File",
      paths.helper.path,
      "-BrowserProcessId",
      String(Services.appinfo.processID),
      "-WindowHandle",
      handle,
      "-Direction",
      direction,
      "-RepeatCount",
      String(repeatCount),
    ],
    stderr: "stdout",
    workdir: paths.repo.path,
  });
  let completed = false;
  let timer = 0;
  try {
    const completion = (async () => {
      let output = "";
      for (;;) {
        const chunk = await process.stdout.readString();
        if (!chunk) break;
        output += chunk;
        assert(output.length <= 65_536, "native helper output is bounded");
      }
      const result = await process.wait();
      assertEquals(result.exitCode, 0, `native helper succeeds: ${output}`);
      const reply = JSON.parse(output.trim()) as NativeInputReply;
      assertEquals(
        reply.status,
        "sent",
        "native helper reports actual delivery",
      );
      assertEquals(
        reply.processId,
        Services.appinfo.processID,
        "browser PID matches",
      );
      assert(
        typeof reply.windowHandle === "string" &&
          /^0x[0-9a-f]+$/i.test(reply.windowHandle) &&
          BigInt(reply.windowHandle) === BigInt(handle),
        "native helper targets the exact HWND",
      );
      assertEquals(reply.direction, direction, "native direction matches");
      assertEquals(
        reply.key,
        direction === "Previous" ? "F13" : "F14",
        "fixed key matches",
      );
      assertEquals(reply.repeatCount, repeatCount, "native repetition matches");
      assertEquals(
        reply.expectedInputCount,
        repeatCount + 5,
        "full key sequence expected",
      );
      assertEquals(
        reply.sentInputCount,
        reply.expectedInputCount,
        "all input was sent",
      );
      assertEquals(reply.cleanupInputCount, 3, "all held keys were released");
      assertEquals(
        reply.foregroundVerified,
        true,
        "native foreground was verified",
      );
      assertEquals(reply.error, null, "helper reports no error");
      completed = true;
      return reply;
    })();
    return await Promise.race([
      completion,
      new Promise<never>((_resolve, reject) => {
        timer = nativeSetTimeout(
          () => reject(new Error("native input helper timed out")),
          30_000,
        );
      }),
    ]);
  } finally {
    nativeClearTimeout(timer);
    if (!completed) await process.kill(0);
  }
}

function snapshot(win: NativeInputWindow): NativeInputSnapshot {
  const tab = win.gBrowser.selectedTab;
  return {
    workspaceId: win.workspacesFuncs!.getSelectedWorkspaceID(),
    selectedTab: tab.getAttribute("linkedpanel") ?? tab.id,
  };
}

function savePref(name: string, kind: "bool" | "string"): NativeInputSavedPref {
  return {
    name,
    hadUserValue: Services.prefs.prefHasUserValue(name),
    value: kind === "bool"
      ? Services.prefs.getBoolPref(name, false)
      : Services.prefs.getStringPref(name, ""),
  };
}

export async function testWindowsNativeWorkspaceShortcuts(): Promise<void> {
  assertEquals(Services.appinfo.OS, "WINNT", "OS-input test runs on Windows");
  const primary = window as unknown as NativeInputWindow;
  await waitFor(
    () => Boolean(Workspaces.getCtx()),
    "workspace service is ready",
  );
  const context = Workspaces.getCtx();
  assert(context && primary.workspacesFuncs, "real workspace service exists");
  const paths = fixturePaths();
  const evidence: NativeInputEvidence = {
    status: "running",
    os: Services.appinfo.OS,
    runtimeVersion: Services.appinfo.version,
    runtimeBuildId: Services.appinfo.appBuildID,
    startedAt: new Date().toISOString(),
    steps: [],
  };
  const saved = [
    savePref(KEYBOARD_SHORTCUT_ENABLED_PREF, "bool"),
    savePref(KEYBOARD_SHORTCUT_CONFIG_PREF, "string"),
    savePref(SIDEBAR_GESTURE_PREF, "bool"),
    savePref(NATIVE_GESTURE_PREF, "bool"),
  ];
  const originalWorkspace = context.getSelectedWorkspaceID();
  const originalTab = primary.gBrowser.selectedTab;
  const originalTabs = new Set(primary.gBrowser.tabs);
  const originalMarkers = [...originalTabs].map((tab) =>
    [tab, tab.getAttribute(WORKSPACE_LAST_SHOW_ID)] as const
  );
  const originalWorkspaces = new Set(workspacesDataStore.order);
  const originalData = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
  const originalFocus = Services.focus.activeWindow as unknown as Window | null;
  const keys: NativeInputKey[] = [];
  const changes: string[] = [];
  const disposers: Array<() => void> = [];
  const failures: unknown[] = [];
  const observer = {
    observe(_subject: nsISupports, _topic: string, data: string) {
      changes.push(data);
    },
  };
  const recordKeys = (
    win: NativeInputWindow,
    name: "primary" | "secondary",
  ) => {
    const listener = (event: KeyboardEvent) => {
      if (!["F13", "F14", "ControlLeft", "ShiftLeft"].includes(event.code)) {
        return;
      }
      keys.push({
        window: name,
        type: event.type,
        code: event.code,
        key: event.key,
        repeat: event.repeat,
        trusted: event.isTrusted,
        ctrl: event.ctrlKey,
        shift: event.shiftKey,
        alt: event.altKey,
        meta: event.metaKey,
        altGraph: event.getModifierState("AltGraph"),
        defaultPrevented: event.defaultPrevented,
      });
    };
    win.addEventListener("keydown", listener, true);
    win.addEventListener("keyup", listener, true);
    disposers.push(() => {
      win.removeEventListener("keydown", listener, true);
      win.removeEventListener("keyup", listener, true);
    });
  };
  const addSelectedTab = (win: NativeInputWindow): XULElement => {
    const tab = win.gBrowser.addTab("about:blank", {
      userContextId: 0,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      skipAnimation: true,
    });
    win.gBrowser.selectedTab = tab;
    return tab;
  };
  const press = async (
    name: string,
    target: "primary" | "secondary",
    win: NativeInputWindow,
    direction: NativeInputDirection,
    expectedWorkspace: string,
    expectedTab: XULElement,
    expectedChanges: number,
    repeatCount = 1,
    background?: NativeInputWindow,
  ) => {
    win.gBrowser.selectedBrowser.focus();
    const keyStart = keys.length;
    const changeStart = changes.length;
    const otherBefore = background ? snapshot(background) : null;
    const step: NativeInputStep = {
      name,
      target,
      direction,
      repeatCount,
      before: snapshot(win),
      keys: [],
      changedWorkspaces: [],
    };
    evidence.steps.push(step);
    await IOUtils.writeJSON(paths.evidence, evidence);
    try {
      step.reply = await sendNativeInput(win, direction, repeatCount);
      await waitFor(
        () =>
          keys.slice(keyStart).some((event) =>
            event.type === "keyup" && !event.ctrl && !event.shift
          ),
        "native key sequence reaches the browser and releases modifiers",
      );
      await settle();
      step.after = snapshot(win);
      step.keys = keys.slice(keyStart);
      step.changedWorkspaces = changes.slice(changeStart);
      const key = direction === "Previous" ? "F13" : "F14";
      const downs = step.keys.filter((event) =>
        event.type === "keydown" && event.code === key
      );
      assertEquals(
        downs.length,
        repeatCount,
        `${name}: expected native keydown count`,
      );
      assert(
        downs.every((event) =>
          event.window === target && event.trusted && event.ctrl &&
          event.shift && !event.alt && !event.meta && !event.altGraph
        ),
        `${name}: only the foreground target receives trusted configured input`,
      );
      for (const [index, event] of downs.entries()) {
        assertEquals(
          event.repeat,
          index > 0,
          `${name}: held keydown ${index + 1} has Gecko's expected repeat flag`,
        );
      }
      assertEquals(
        win.workspacesFuncs!.getSelectedWorkspaceID(),
        expectedWorkspace,
        `${name}: expected workspace`,
      );
      assertEquals(
        win.gBrowser.selectedTab,
        expectedTab,
        `${name}: last selected tab is restored`,
      );
      assertEquals(
        step.changedWorkspaces.length,
        expectedChanges,
        `${name}: exact workspace switch count`,
      );
      if (background) {
        assert(otherBefore, `${name}: background state was recorded`);
        const otherAfter = snapshot(background);
        assertEquals(
          otherAfter.workspaceId,
          otherBefore.workspaceId,
          `${name}: background workspace stays unchanged`,
        );
        assertEquals(
          otherAfter.selectedTab,
          otherBefore.selectedTab,
          `${name}: background selected tab stays unchanged`,
        );
      }
    } finally {
      step.keys = keys.slice(keyStart);
      step.changedWorkspaces = changes.slice(changeStart);
      step.after = snapshot(win);
      await IOUtils.writeJSON(paths.evidence, evidence);
    }
  };
  try {
    Services.prefs.setBoolPref(SIDEBAR_GESTURE_PREF, false);
    Services.prefs.setBoolPref(NATIVE_GESTURE_PREF, false);
    Services.prefs.setStringPref(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
      JSON.stringify({
        schemaVersion: 2,
        enabled: true,
        shortcuts: Object.fromEntries([
          ["Previous", "F13", "floorp-workspace-previous-no-wrap"],
          ["Next", "F14", "floorp-workspace-next-no-wrap"],
        ].map((
          [name, key, action],
        ) => [name, {
          key,
          action,
          modifiers: { ctrl: true, shift: true, alt: false, meta: false },
        }])),
      }),
    );
    Services.prefs.setBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF, true);
    await settle();
    await waitFor(
      () => primary.__keyboardShortcutControllerAttached === true,
      "production shortcut controller is attached",
    );
    recordKeys(primary, "primary");
    Services.obs.addObserver(observer, WORKSPACES_CHANGED_OBSERVER_TOPIC);
    disposers.push(() =>
      Services.obs.removeObserver(observer, WORKSPACES_CHANGED_OBSERVER_TOPIC)
    );
    const a = workspacesDataStore.order.at(-1);
    assert(a, "at least one existing workspace exists");
    context.changeWorkspace(a);
    addSelectedTab(primary);
    const aTab = addSelectedTab(primary);
    const b = context.createNoNameWorkspace();
    const bTab = addSelectedTab(primary);
    const c = context.createNoNameWorkspace();
    const cTab = addSelectedTab(primary);
    context.changeWorkspace(b);
    await press("B to C", "primary", primary, "Next", c, cTab, 1);
    await press("C to B", "primary", primary, "Previous", b, bTab, 1);
    await press("B to A", "primary", primary, "Previous", a, aTab, 1);
    await press(
      "held next advances once",
      "primary",
      primary,
      "Next",
      b,
      bTab,
      1,
      3,
    );
    await press("B to final C", "primary", primary, "Next", c, cTab, 1);
    await press(
      "next stops at final C",
      "primary",
      primary,
      "Next",
      c,
      cTab,
      0,
    );
    const first = workspacesDataStore.order[0];
    context.changeWorkspace(first);
    await press(
      "previous stops at global start",
      "primary",
      primary,
      "Previous",
      first,
      primary.gBrowser.selectedTab,
      0,
    );
    await withWindows(false, false, async (open) => {
      const secondary = await open() as NativeInputWindow;
      await waitFor(
        () => secondary.__keyboardShortcutControllerAttached === true,
        "second window has its production shortcut controller",
      );
      recordKeys(secondary, "secondary");
      secondary.workspacesFuncs!.changeWorkspace(b);
      const secondaryB = addSelectedTab(secondary);
      secondary.workspacesFuncs!.changeWorkspace(a);
      addSelectedTab(secondary);
      context.changeWorkspace(b);
      await press(
        "foreground secondary only",
        "secondary",
        secondary,
        "Next",
        b,
        secondaryB,
        1,
        1,
        primary,
      );
      await press(
        "foreground primary only",
        "primary",
        primary,
        "Previous",
        a,
        aTab,
        1,
        1,
        secondary,
      );
    });
  } catch (error) {
    failures.push(error);
  } finally {
    for (const dispose of disposers.reverse()) {
      try {
        dispose();
      } catch (error) {
        failures.push(error);
      }
    }
    try {
      context.changeWorkspace(originalWorkspace);
      primary.gBrowser.selectedTab = originalTab;
      for (const id of [...workspacesDataStore.order]) {
        if (!originalWorkspaces.has(id)) context.deleteWorkspace(id);
      }
      for (const tab of [...primary.gBrowser.tabs]) {
        if (!originalTabs.has(tab)) {
          primary.gBrowser.removeTab(tab, { animate: false });
        }
      }
      Services.prefs.setStringPref(WORKSPACE_DATA_PREF_NAME, originalData);
      context.changeWorkspace(originalWorkspace);
      primary.gBrowser.selectedTab = originalTab;
      for (const [tab, marker] of originalMarkers) {
        if (marker === null) tab.removeAttribute(WORKSPACE_LAST_SHOW_ID);
        else tab.setAttribute(WORKSPACE_LAST_SHOW_ID, marker);
      }
    } catch (error) {
      failures.push(error);
    }
    for (const pref of saved) {
      try {
        if (!pref.hadUserValue) Services.prefs.clearUserPref(pref.name);
        else if (typeof pref.value === "boolean") {
          Services.prefs.setBoolPref(pref.name, pref.value);
        } else Services.prefs.setStringPref(pref.name, pref.value);
      } catch (error) {
        failures.push(error);
      }
    }
    try {
      originalFocus?.focus();
    } catch (error) {
      failures.push(error);
    }
    evidence.status = failures.length ? "failed" : "passed";
    evidence.finishedAt = new Date().toISOString();
    if (failures.length) evidence.error = failures.map(String).join(" | ");
    await IOUtils.writeJSON(paths.evidence, evidence);
  }
  if (failures.length) {
    throw new AggregateError(
      failures,
      "Windows native workspace shortcut verification failed",
    );
  }
}
