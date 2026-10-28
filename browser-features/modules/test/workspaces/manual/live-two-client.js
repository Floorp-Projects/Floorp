// SPDX-License-Identifier: MPL-2.0
// Live acceptance only: two separately authorized, dedicated QA profiles.
// deno-lint-ignore no-unused-vars
async function liveOperation(action, args) {
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  if (
    Services.dirsvc.get("ProfD", Ci.nsIFile).path !== args.profile ||
    Services.prefs.getStringPref(
        "floorp.workspaces.sync.acceptance-profile",
        "",
      ) !== args.client
  ) throw new Error("Dedicated QA profile mismatch");
  if (Services.prefs.getBoolPref("remote.prefs.recommended", true)) {
    throw new Error(
      "Live QA must disable automation recommended preferences before browser startup",
    );
  }
  const expectedEndpoints = {
    "identity.fxaccounts.auth.uri": "https://api.accounts.firefox.com/v1",
    "identity.fxaccounts.remote.root": "https://accounts.firefox.com/",
    "identity.fxaccounts.remote.oauth.uri":
      "https://oauth.accounts.firefox.com/v1",
    "identity.fxaccounts.remote.profile.uri":
      "https://profile.accounts.firefox.com/v1",
    "identity.sync.tokenserver.uri":
      "https://token.services.mozilla.com/1.0/sync/1.5",
  };
  const isExpectedEndpoint = (value, expected) => {
    try {
      const actual = new URL(value), official = new URL(expected);
      return !actual.username && !actual.password && !actual.search &&
        !actual.hash &&
        actual.origin === official.origin &&
        actual.pathname.replace(/\/$/, "") ===
          official.pathname.replace(/\/$/, "");
    } catch {
      return false;
    }
  };
  for (const [key, expected] of Object.entries(expectedEndpoints)) {
    if (!isExpectedEndpoint(Services.prefs.getStringPref(key, ""), expected)) {
      throw new Error(
        "Live QA account endpoints must use the official production configuration",
      );
    }
  }
  const { getFxAccountsSingleton: configuredFxA } = ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs",
  );
  const configuredClient = configuredFxA()._internal.fxAccountsClient;
  if (
    !isExpectedEndpoint(
      configuredClient.host,
      expectedEndpoints["identity.fxaccounts.auth.uri"],
    ) ||
    !isExpectedEndpoint(
      configuredClient.hawk.host,
      expectedEndpoints["identity.fxaccounts.auth.uri"],
    )
  ) {
    throw new Error(
      "A cached native FxA client still uses a different endpoint",
    );
  }
  const { Service } = ChromeUtils.importESModule(
    "resource://services-sync/service.sys.mjs",
  );
  await Service.promiseInitialized;
  const engines = Service.engineManager.getAll().map((engine) => engine.name);
  if (engines.length !== 1 || engines[0] !== "floorp-workspaces-v2") {
    throw new Error("QA must register only the workspace engine");
  }
  if (action === "preflight") {
    return {
      authConfigurationChecked: true,
      recommendedPrefsDisabled: true,
      scopedEngineRegistration: true,
    };
  }
  const { FloorpWorkspacesSync: api } = ChromeUtils.importESModule(
    "resource://noraneko/modules/workspaces/FloorpWorkspacesSync.sys.mjs",
  );
  const { SessionStore } = ChromeUtils.importESModule(
    "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
  );
  await api.ready();
  const engine = Service.engineManager.get("floorp-workspaces-v2");
  const { WorkspaceContainerBindings: bindings } = ChromeUtils.importESModule(
    "resource://noraneko/modules/workspaces/WorkspaceContainerBindings.sys.mjs",
  );
  const { ContextualIdentityService: identities } = ChromeUtils.importESModule(
    "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
  );
  const tabs = (id) =>
    [...win.gBrowser.tabs].filter((tab) =>
      tab.getAttribute("floorpWorkspaceId") === id
    );
  const metadata = (id) => {
    const store = JSON.parse(
      Services.prefs.getStringPref("floorp.workspaces.v4.store"),
    );
    return store.data.find(([key]) => key === id)?.[1] ?? null;
  };
  const editMetadata = (id, update) => {
    const store = JSON.parse(
      Services.prefs.getStringPref("floorp.workspaces.v4.store"),
    );
    store.data = store.data.map((
      [key, value],
    ) => [key, key === id ? { ...value, ...update } : value]);
    Services.prefs.setStringPref(
      "floorp.workspaces.v4.store",
      JSON.stringify(store),
    );
  };
  const add = (id, url, userContextId) => {
    win.workspacesFuncs.changeWorkspace(id);
    const before = new Set(win.gBrowser.tabs);
    win.openTrustedLinkIn(url, "tab", {
      inBackground: true,
      skipAnimation: true,
      ...(userContextId === undefined ? {} : { userContextId }),
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    const tab = [...win.gBrowser.tabs].find((candidate) =>
      !before.has(candidate)
    );
    if (!tab) throw new Error("QA native tab creation did not succeed");
    tab.setAttribute("floorpWorkspaceId", id);
    return tab;
  };
  const findTab = (id, tabId) =>
    tabs(id).find((tab) =>
      SessionStore.getCustomTabValue(tab, "floorp-workspace-sync-id") === tabId
    );
  const waitPages = async (id, expectedURLs = []) => {
    for (let attempt = 0; attempt < 80; attempt++) {
      const web = tabs(id).filter((tab) =>
        /^https?:/.test(
          win.gBrowser.getBrowserForTab(tab).currentURI?.spec ?? "",
        )
      );
      if (
        expectedURLs.every((url) =>
          web.some((tab) =>
            win.gBrowser.getBrowserForTab(tab).currentURI?.spec === url
          )
        ) &&
        !tabs(id).some((tab) => {
          const pending = SessionStore.getCustomTabValue(
            tab,
            "floorp-workspace-sync-url",
          );
          return /^https?:/.test(pending) &&
            win.gBrowser.getBrowserForTab(tab).currentURI?.spec !== pending;
        }) &&
        web.every((tab) => {
          const browser = win.gBrowser.getBrowserForTab(tab);
          return !browser.webProgress?.isLoadingDocument &&
            browser.contentTitle?.startsWith("QA ");
        })
      ) return;
      await new Promise((resolve) => win.setTimeout(resolve, 100));
    }
    throw new Error("QA pages did not finish loading");
  };
  if (action === "ready") {
    const { getFxAccountsSingleton } = ChromeUtils.importESModule(
      "resource://gre/modules/FxAccounts.sys.mjs",
    );
    const user = await getFxAccountsSingleton().getSignedInUser();
    // The host compares UID in memory, and only records the equality boolean.
    const { SCOPE_APP_SYNC } = ChromeUtils.importESModule(
      "resource://gre/modules/FxAccountsCommon.sys.mjs",
    );
    const fxa = getFxAccountsSingleton();
    return {
      signedIn: Boolean(user),
      verified: Boolean(user?.verified),
      syncReady: Boolean(user) &&
        await fxa.keys.hasKeysForScope(SCOPE_APP_SYNC) &&
        Boolean(Services.prefs.getStringPref("services.sync.username", "")),
      uid: user?.uid ?? null,
      engines,
    };
  }
  if (action === "pause-scheduler") {
    win.__floorpLiveSyncSchedule ??= Service.scheduler.scheduleNextSync;
    Service.scheduler.clearSyncTriggers();
    Service.scheduler.scheduleNextSync = () => {};
    return { paused: true };
  }
  if (action === "restore-scheduler") {
    if (win.__floorpLiveSyncSchedule) {
      Service.scheduler.scheduleNextSync = win.__floorpLiveSyncSchedule;
      delete win.__floorpLiveSyncSchedule;
      Service.scheduler.scheduleNextSync();
    }
    return { restored: true };
  }
  if (action === "create") {
    const id = win.workspacesFuncs.createNoNameWorkspace();
    editMetadata(id, {
      name: args.name,
      icon: "book",
      ...(args.defaultUserContextId === undefined
        ? {}
        : { userContextId: args.defaultUserContextId }),
    });
    if (args.defaultUserContextId !== undefined) {
      let applied = false;
      for (let attempt = 0; attempt < 100; attempt++) {
        if (
          win.workspacesFuncs.getCurrentWorkspaceUserContextId() ===
            args.defaultUserContextId
        ) {
          applied = true;
          break;
        }
        await new Promise((resolve) => win.setTimeout(resolve, 50));
      }
      if (!applied) throw new Error("QA workspace default did not update");
    }
    for (const [index, url] of args.urls.entries()) {
      add(id, url, args.tabUserContextIds?.[index] ?? undefined);
    }
    await waitPages(id, args.urls);
    if (args.selected) await api.select(id, true);
    await api.capture();
    return { workspaceId: id };
  }
  if (action === "snapshot") {
    const local = metadata(args.workspaceId);
    const desired = api.getSyncedWorkspace(args.workspaceId);
    const refs = [
      desired?.defaultContainer,
      ...(desired?.tabs ?? []).filter((tab) => !tab.deleted).map((tab) =>
        tab.container
      ),
    ];
    const containerIds = new Set(
      refs.filter((ref) => ref?.kind === "container").map((ref) =>
        ref.containerId
      ),
    );
    return {
      exists: Boolean(local),
      name: local?.name,
      icon: local?.icon,
      selected: api.isSelected(args.workspaceId),
      error: api.errorFor(args.workspaceId),
      defaultUserContextId: local?.userContextId ?? 0,
      desiredDefaultContainer: desired?.defaultContainer ?? null,
      desiredTabs: (desired?.tabs ?? []).map(({ id, container, deleted }) => ({
        id,
        container,
        deleted,
      })),
      containerStates: [...containerIds].map((containerId) => {
        const resolution = bindings.lookup({ kind: "container", containerId });
        return {
          containerId,
          status: resolution.status,
          ...(resolution.status === "resolved"
            ? { userContextId: resolution.userContextId }
            : { reason: resolution.reason }),
        };
      }),
      tabs: tabs(args.workspaceId).map((tab) => {
        const browser = win.gBrowser.getBrowserForTab(tab);
        return {
          id: SessionStore.getCustomTabValue(tab, "floorp-workspace-sync-id"),
          url: browser.currentURI?.spec,
          title: browser.contentTitle,
          pinned: tab.hasAttribute("pinned"),
          userContextId: Number(tab.getAttribute("usercontextid") || 0),
        };
      }).filter((tab) => /^https?:/.test(tab.url ?? "")),
    };
  }
  if (action === "container-create") {
    Services.prefs.setBoolPref("privacy.userContext.enabled", true);
    let identity = identities.create(
      args.name,
      args.icon ?? "briefcase",
      args.color ?? "blue",
    );
    if (identity.userContextId === args.avoidId) {
      identities.update(
        identity.userContextId,
        `QA ID offset ${args.name}`,
        identity.icon,
        identity.color,
      );
      identity = identities.create(
        args.name,
        args.icon ?? "briefcase",
        args.color ?? "blue",
      );
    }
    await identities.save();
    return {
      userContextId: identity.userContextId,
      name: identity.name,
      color: identity.color,
      icon: identity.icon,
    };
  }
  if (action === "container-info") {
    return bindings.listLocalContainers().filter((identity) =>
      args.userContextId === undefined
        ? identity.name === args.name
        : identity.userContextId === args.userContextId
    );
  }
  if (action === "container-bind") {
    await bindings.bind(
      args.containerId,
      args.userContextId,
      bindings.getEpoch(),
    );
    await api.capture();
    return {
      bound:
        bindings.lookup({ kind: "container", containerId: args.containerId })
          .status === "resolved",
    };
  }
  if (action === "container-rename") {
    const identity = identities.getPublicIdentityFromId(args.userContextId);
    if (!identity) throw new Error("QA native container missing");
    identities.update(
      identity.userContextId,
      args.name,
      identity.icon,
      identity.color,
    );
    await identities.save();
    await api.capture();
    return { renamed: true };
  }
  if (action === "container-delete") {
    const removal = await identities.prepareRemoval(
      args.userContextId,
      "delete",
    );
    let outcome = "cancelled";
    try {
      await identities.closeContainerTabs(args.userContextId);
      if (
        identities.countContainerTabs(args.userContextId) !== 0 ||
        !identities.remove(args.userContextId, removal)
      ) {
        throw new Error("QA native container deletion was cancelled");
      }
      outcome = "completed";
    } finally {
      await identities.finishRemoval(removal, outcome);
    }
    await api.capture();
    return { deleted: true };
  }
  if (action === "container-create-binding") {
    await bindings.create(args.containerId, bindings.getEpoch());
    await api.capture();
    return bindings.lookup({
      kind: "container",
      containerId: args.containerId,
    });
  }
  if (action === "container-reopen") {
    const operationId = args.operationId;
    if (!/^[0-9a-f-]{36}$/.test(operationId ?? "")) {
      throw new Error("QA reopen operation token required");
    }
    if (win.__floorpQaLastContainerReopen?.complete === false) {
      throw new Error("Another QA reopen operation is still active");
    }
    const runReopen = async () => {
      win.__floorpQaLastContainerReopen = { phase: "initializing" };
      win.workspacesFuncs.changeWorkspace(args.workspaceId);
      const source = findTab(args.workspaceId, args.tabId);
      const workspaceNode = win.document.getElementById(
        `workspace-${args.workspaceId}`,
      );
      const popup = win.document.getElementById(
        "workspaces-toolbar-item-context-menu",
      );
      if (!source || !workspaceNode || !popup) {
        throw new Error("QA container reopen UI is unavailable");
      }
      const oldPopupNode = win.document.popupNode;
      let selected = false, confirmed = false, alerted = false;
      let promptsActive = true;
      let phase = "opening-menu";
      const expectedTitles = new Set([
        "Workspace Containers",
        "ワークスペースのコンテナ",
      ]);
      // Operate the real native dialogs. Prompt-service monkeypatches on a
      // Marionette sandbox's XPCOM wrapper do not affect browser-chrome's realm.
      const acceptWorkspacePrompt = (subject, topic) => {
        const dialogWindow = subject.wrappedJSObject ?? subject;
        const document = dialogWindow.document;
        const title = topic === "select-dialog-loaded"
          ? document.title
          : dialogWindow.args?.title;
        if (!expectedTitles.has(title)) return;
        if (
          document.documentURI !==
            (topic === "select-dialog-loaded"
              ? "chrome://global/content/selectDialog.xhtml"
              : "chrome://global/content/commonDialog.xhtml")
        ) return;
        if (!promptsActive || dialogWindow.closed) return;
        const dialog = document.querySelector("dialog");
        if (topic === "select-dialog-loaded") {
          phase = "select-dialog";
          const list = document.getElementById("list");
          const labels = Array.from(
            { length: list.itemCount },
            (_, index) =>
              list.getItemAtIndex(index).querySelector("label")?.getAttribute(
                "value",
              ),
          );
          const index = labels.indexOf(source.getAttribute("label"));
          if (index < 0) {
            phase = "selection-label-missing";
            alerted = true;
            dialog.cancelDialog();
            return;
          }
          list.selectedIndex = index;
          selected = true;
        } else if (dialogWindow.args.promptType === "confirm") {
          phase = "confirm-dialog";
          confirmed = true;
        } else if (dialogWindow.args.promptType === "alert") {
          phase = "product-alert";
          alerted = true;
        } else {
          return;
        }
        // These observers run after native handlers are installed. Synchronous
        // acceptance also works while nsIPromptService spins its modal event loop.
        dialog.acceptDialog();
      };
      Services.obs.addObserver(acceptWorkspacePrompt, "select-dialog-loaded");
      Services.obs.addObserver(acceptWorkspacePrompt, "common-dialog-loaded");
      try {
        win.document.popupNode = workspaceNode;
        popup.dispatchEvent(new win.Event("popupshowing", { bubbles: true }));
        await Promise.resolve();
        const item = popup.querySelector(
          '[data-floorp-context-menu-key="floorp.workspaces.containers-reopen"]',
        );
        if (!item || item.hidden) {
          throw new Error("QA explicit reopen command is unavailable");
        }
        item.doCommand();
        if (!alerted) phase = "waiting-for-replacement";
        let reopened = false;
        for (let attempt = 0; attempt < 200; attempt++) {
          const target = findTab(args.workspaceId, args.tabId);
          if (
            target && target !== source &&
            Number(target.getAttribute("usercontextid") || 0) ===
              args.userContextId &&
            ![...win.gBrowser.tabs].includes(source)
          ) {
            reopened = true;
            break;
          }
          if (alerted) break;
          await new Promise((resolve) => win.setTimeout(resolve, 100));
        }
        if (!selected || !confirmed || alerted || !reopened) {
          throw new Error("QA explicit native reopen did not finish");
        }
        await waitPages(args.workspaceId);
        await api.capture();
        phase = "completed";
        return {
          reopened,
          stableUUID: tabs(args.workspaceId).filter((tab) =>
            SessionStore.getCustomTabValue(tab, "floorp-workspace-sync-id") ===
              args.tabId
          ).length === 1,
        };
      } finally {
        win.__floorpQaLastContainerReopen = {
          operationId,
          complete: false,
          phase,
          selected,
          confirmed,
          alerted,
          sourceConnected: [...win.gBrowser.tabs].includes(source),
          uuidOwners: tabs(args.workspaceId).filter((tab) =>
            SessionStore.getCustomTabValue(tab, "floorp-workspace-sync-id") ===
              args.tabId
          ).length,
        };
        promptsActive = false;
        Services.obs.removeObserver(
          acceptWorkspacePrompt,
          "select-dialog-loaded",
        );
        Services.obs.removeObserver(
          acceptWorkspacePrompt,
          "common-dialog-loaded",
        );
        popup.dispatchEvent(new win.Event("popuphiding", { bubbles: true }));
        win.document.popupNode = oldPopupNode;
      }
    };
    win.__floorpQaLastContainerReopen = {
      operationId,
      phase: "scheduled",
      complete: false,
      passed: false,
    };
    // Return the Marionette command before native prompts enter their modal
    // event loops. The driver polls this token's plain diagnostic state using
    // separate synchronous commands, rather than keeping an async script open.
    win.setTimeout(() => {
      runReopen().then((result) => {
        win.__floorpQaLastContainerReopen = {
          ...win.__floorpQaLastContainerReopen,
          operationId,
          complete: true,
          passed: true,
          ...result,
        };
      }, () => {
        // Native exception messages may include account data; retain only the
        // controlled phase and primitive outcome flags from the QA operation.
        win.__floorpQaLastContainerReopen = {
          ...win.__floorpQaLastContainerReopen,
          operationId,
          complete: true,
          passed: false,
        };
      });
    }, 100);
    return { started: true, operationId };
  }
  if (action === "edit") {
    const beforeOrder = tabs(args.workspaceId);
    if (args.name) editMetadata(args.workspaceId, { name: args.name });
    if (args.addURL) add(args.workspaceId, args.addURL);
    if (args.tabId) {
      const tab = findTab(args.workspaceId, args.tabId);
      if (!tab) throw new Error("QA target tab is missing");
      if (args.url) {
        win.gBrowser.getBrowserForTab(tab).loadURI(
          Services.io.newURI(args.url),
          {
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
          },
        );
      }
      if (args.pinned === true) win.gBrowser.pinTab(tab);
      if (args.pinned === false) win.gBrowser.unpinTab(tab);
      if (args.first) {
        const first = tabs(args.workspaceId).find((candidate) =>
          candidate !== tab && !candidate.hasAttribute("pinned")
        );
        if (first) {
          win.gBrowser.moveTabTo(tab, {
            tabIndex: [...win.gBrowser.tabs].indexOf(first),
          });
        }
      }
      if (args.last) {
        const last = tabs(args.workspaceId).at(-1);
        if (last && last !== tab) {
          win.gBrowser.moveTabTo(tab, {
            tabIndex: [...win.gBrowser.tabs].indexOf(last),
          });
        }
      }
      if (args.close) win.gBrowser.removeTab(tab, { animate: false });
    }
    await waitPages(args.workspaceId, [args.url, args.addURL].filter(Boolean));
    await api.capture();
    return {
      edited: true,
      reordered: tabs(args.workspaceId).some((tab, index) =>
        tab !== beforeOrder[index]
      ),
    };
  }
  if (action === "select") {
    await api.select(args.workspaceId, args.selected);
    return { selected: api.isSelected(args.workspaceId) };
  }
  if (action === "delete") {
    await api.deleteWorkspace(args.workspaceId);
    return { selected: api.isSelected(args.workspaceId) };
  }
  if (action === "settings-fingerprint") {
    if (!Service.storageURL) await Service.login();
    const response = await Service.resource(Service.metaURL).get();
    if (!response.success && response.status !== 404) {
      throw new Error("Could not read account metadata fingerprint");
    }
    const outer = response.status === 404 ? { payload: {} } : response.obj;
    const payload = typeof outer.payload === "string"
      ? JSON.parse(outer.payload)
      : outer.payload;
    const otherEngines = Object.entries(payload.engines ?? {}).filter((
      [name],
    ) => name !== "clients" && name !== "floorp-workspaces-v2").sort((
      [a],
      [b],
    ) => a.localeCompare(b));
    const declined = (payload.declined ?? []).filter((name) =>
      name !== "floorp-workspaces-v2"
    ).sort();
    const bytes = new TextEncoder().encode(
      JSON.stringify({ otherEngines, declined }),
    );
    const digest = await win.crypto.subtle.digest("SHA-256", bytes);
    return {
      fingerprint: Array.from(new Uint8Array(digest)).map((byte) =>
        byte.toString(16).padStart(2, "0")
      ).join(""),
    };
  }
  if (action === "sync") {
    if (!Service.storageURL) await Service.login();
    const endpoint = new URL(Service.storageURL);
    if (endpoint.protocol !== "https:" || endpoint.hostname === "localhost") {
      throw new Error("Live acceptance requires authenticated HTTPS storage");
    }
    for (let attempt = 0; attempt < 4; attempt++) {
      let finished = false, failed = false;
      const observer = (_subject, topic, name) => {
        if (name !== "floorp-workspaces-v2") return;
        if (topic.endsWith(":finish")) finished = true;
        if (topic.endsWith(":error")) failed = true;
      };
      Services.obs.addObserver(observer, "weave:engine:sync:finish");
      Services.obs.addObserver(observer, "weave:engine:sync:error");
      try {
        await api.capture();
        await Service.sync({ engines: ["floorp-workspaces-v2"], why: "user" });
        if (failed) throw new Error("Workspace Sync engine reported an error");
        if (finished) {
          return {
            finished: true,
            httpsStorage: true,
            host: endpoint.hostname,
            lastSyncPositive: (await engine.getLastSync()) > 0,
          };
        }
      } finally {
        Services.obs.removeObserver(observer, "weave:engine:sync:finish");
        Services.obs.removeObserver(observer, "weave:engine:sync:error");
      }
      await new Promise((resolve) => win.setTimeout(resolve, 1000));
    }
    throw new Error("Workspace engine did not complete a live sync");
  }
  throw new Error("Unknown QA operation");
}
