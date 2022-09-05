# Browser Toolbox

## Introduction

The Browser Toolbox spawns a toolbox in a new dedicated Firefox instance to debug the currently running Firefox. This new instance runs in a distinct process.

To enable it, you must first flip two preferences in the DevTools Options panel (F1):
- Enable browser chrome and add-on debugging toolboxes
- Enable remote debugging

You can either start it via a keyboard shortcut (CmdOrCtrl+Alt+Shift+I) or via the Tools > Web Developer > Browser Toolbox menu item.

When describing the setup used by the Browser Toolbox, we will refer to those two distinct Firefox instances as:
- the target Firefox: this is the current instance, that we want to debug
- the client Firefox: this is the new instance that will only run the Browser Toolbox window

## Browser Toolbox Architecture

The startup sequence of the browser toolbox begins in the target Firefox.

`browser-toolbox/Launcher.sys.mjs` will be first reponsible for creating a remote DevToolsServer. This new DevToolsServer runs in the parent process but is separated from any existing DevTools DevToolsServer that spawned earlier for regular DevTools usage. Thanks to this, we will be able to debug files loaded in those regular DevToolsServers used for content toolboxes, about:debugging, ...

Then we need to start the client Firefox. To do that, `browser-toolbox/Launcher.sys.mjs` creates a profile that will be a copy of the current profile loaded in the target Firefox, so that all user preferences can be automatically ported over. As a reminder both client and target Firefox will run simultaneously, so they can't use the same profile.

This new profile is stored inside the folder of the target profile, in a `chrome_debugger_profile` folder. So the next time the Browser Toolbox opens this for profile, it will be reused.

Once the profile is ready (or if it was already there), `browser-toolbox/Launcher.sys.mjs` spawns a new Firefox instance with a few additional parameters, most importantly `-chrome chrome://devtools/content/framework/browser-toolbox/window.html`.

This way Firefox will load `browser-toolbox/window.html` instead of the regular browser window. Most of the logic is then handled by `browser-toolbox/window.js` which will connect to the remote server opened on the target Firefox and will then load a toolbox connected to this server.

## Debugging the Browser Toolbox

Note that you can open a Browser Toolbox from the Browser Toolbox. Simply reuse the same shortcut as the one you used to open the first Browser Toolbox, but this time while the Browser Toolbox window is focused.

Another Browser Toolbox will spawn, this time debugging the first Browser Toolbox Firefox instance. If you are curious about how this is done, `browser-toolbox/window.js` simply loads `browser-toolbox/Launcher.sys.mjs` and requests to open a new Browser Toolbox.

This will open yet another Firefox instance, running in another process. And a new `chrome_debugger_profile` folder will be created inside the existing Browser Toolbox profile (which as explained in the previous section, is already in a `chrome_debugger_profile` folder under the target Firefox profile).
