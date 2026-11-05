# Wayland ordinary-browser residual: read-only source assessment

Source: Floorp PR3032 head 8249e234adfbb3e877e4a20cf5f57d7264d9a447; locked Runtime 8965ffc346d426ccdf919bd18585e4d4a0c21654. No source/product edits or new runtime reproduction performed in this assessment. The residual was reproduced in our prior completed CI package run42068055038. Cloud B strict-XPT package work was mentioned by the user; no Cloud B artifact or additional residual report was inspected here, and its output is not treated as the same product.

## Minimal proposed integration

In the Linux --start-ssb handler, register a process-local browser-window-before-initial-xul-layout-document-preparation category listener only when a start-ssb argument exists and cmdLine.state is STATE_INITIAL_LAUNCH. Register before calling the asynchronous start routine. Use resource://noraneko/modules/pwa/supports/Linux.sys.mjs as module entry and LinuxSupport.prepareBrowserWindow as callback.

The synchronous callback should leave taskbartab windows and any preexisting nonempty windowclass unchanged; otherwise set windowclass to Services.appinfo.remotingName, falling back to AppConstants.MOZ_APP_NAME when empty. Do not enable or call experimental native taskbar interfaces.

## Source evidence

- browser/base/content/browser-init.js: taskbartabclass is copied to windowclass and taskbartab is applied before calling this category, before initial layout.
- toolkit/modules/BrowserUtils.sys.mjs: non-idle category listeners invoke imported ES module methods immediately. Listener method receives window, not jsGlobal. Callback must not defer the attribute mutation with await/timers. Runtime category changes invalidate its listener cache.
- toolkit/modules/AppConstants.sys.mjs and packaged modules/AppConstants.sys.mjs: MOZ_APP_NAME exists (floorp in the package), but MOZ_APP_REMOTINGNAME is not exported.
- xpcom/system/nsIXULAppInfo.idl: remotingName exposes XREAppData.remotingName.
- toolkit/xre/nsAppRunner.cpp: g_set_prgname(gAppData->remotingName) precedes gtk_parse_args. This supports remotingName as canonical identity rather than a hardcoded floorp or localized product name.
- widget/gtk/nsWindow.cpp: explicit nonempty xulWinClass becomes mGtkWindowAppClass; RefreshWindowClass forwards it to gdk_wayland_window_set_application_id for the toplevel. Existing native path suffices.
- Floorp SsbCommandLineHandler.isInitialized does not represent process initial command state: it stays false through ordinary browser startup. Use cmdLine.state to avoid registering on the first remote PWA request into a browser-first process.

## Risks and required runtime validation

A PWA-first command with explicit custom --name may assign later normal windows canonical remoting identity rather than that requested name. GTK parses/removes its options earlier, so do not assume nsICommandLine retains --name. Normal browser-first custom names are preserved by the proposed scope. Invalid initial PWA IDs would still register the callback if registration occurs before async ID resolution. These are compatibility tradeoffs to document or cover deliberately.

Retain PR3032 PWA A/B identity and rollout-gate tests; add callback guards, initial-vs-remote command scope, and synchronous attribute tests. Source tests alone do not prove compositor behavior. Actual Wayland PWA-first A/B followed by normal windows must demonstrate normal canonical app_id and distinct PWA app_ids; preserve custom class and browser-first behavior. Private windows and other browser-root windows share this callback and merit focused regression coverage.

No native rebuild is required by this proposal. No protections, signature rules, rollout gates, WM rules, or process-global GTK identity should be changed.
