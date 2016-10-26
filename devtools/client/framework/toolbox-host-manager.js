const Services = require("Services");
const {Ci} = require("chrome");
const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const {Task} = require("devtools/shared/task");

loader.lazyRequireGetter(this, "Toolbox", "devtools/client/framework/toolbox", true);
loader.lazyRequireGetter(this, "Hosts", "devtools/client/framework/toolbox-hosts", true);

/**
 * Implement a wrapper on the chrome side to setup a Toolbox within Firefox UI.
 *
 * This component handles iframe creation within Firefox, in which we are loading
 * the toolbox document. Then both the chrome and the toolbox document communicate
 * via "message" events.
 *
 * Messages sent by the toolbox to the chrome:
 * - switch-host:
 *   Order to display the toolbox in another host (side, bottom, window, or the
 *   previously used one)
 * - toggle-minimize-mode:
 *   When using the bottom host, the toolbox can be miximized to only display
 *   the tool titles
 * - maximize-host:
 *   When using the bottom host in minimized mode, revert back to regular mode
 *   in order to see tool titles and the tools
 * - raise-host:
 *   Focus the tools
 * - set-host-title:
 *   When using the window host, update the window title
 *
 * Messages sent by the chrome to the toolbox:
 * - host-minimized:
 *   The bottom host is done minimizing (after animation end)
 * - host-maximized:
 *   The bottom host is done switching back to regular mode (after animation
 *   end)
 * - switched-host:
 *   The `switch-host` command sent by the toolbox is done
 */

const LAST_HOST = "devtools.toolbox.host";
const PREVIOUS_HOST = "devtools.toolbox.previousHost";
let ID_COUNTER = 1;

function ToolboxHostManager(target, hostType, hostOptions) {
  this.target = target;

  this.frameId = ID_COUNTER++;

  if (!hostType) {
    hostType = Services.prefs.getCharPref(LAST_HOST);
  }
  this.onHostMinimized = this.onHostMinimized.bind(this);
  this.onHostMaximized = this.onHostMaximized.bind(this);
  this.host = this.createHost(hostType, hostOptions);
  this.hostType = hostType;
}

ToolboxHostManager.prototype = {
  create: Task.async(function* (toolId) {
    yield this.host.create();

    this.host.frame.setAttribute("aria-label", L10N.getStr("toolbox.label"));
    this.host.frame.ownerDocument.defaultView.addEventListener("message", this);
    // We have to listen on capture as no event fires on bubble
    this.host.frame.addEventListener("unload", this, true);

    let toolbox = new Toolbox(this.target, toolId, this.host.type, this.host.frame.contentWindow, this.frameId);

    // Prevent reloading the toolbox when loading the tools in a tab (e.g. from about:debugging)
    if (!this.host.frame.contentWindow.location.href.startsWith("about:devtools-toolbox")) {
      this.host.frame.setAttribute("src", "about:devtools-toolbox");
    }

    return toolbox;
  }),

  handleEvent(event) {
    switch(event.type) {
      case "message":
        this.onMessage(event);
        break;
      case "unload":
        // On unload, host iframe already lost its contentWindow attribute, so
        // we can only compare against locations. Here we filter two very
        // different cases: preliminary about:blank document as well as iframes
        // like tool iframes.
        if (!event.target.location.href.startsWith("about:devtools-toolbox")) {
          break;
        }
        // Don't destroy the host during unload event (esp., don't remove the
        // iframe from DOM!). Otherwise the unload event for the toolbox
        // document doesn't fire within the toolbox *document*! This is
        // the unload event that fires on the toolbox *iframe*.
        DevToolsUtils.executeSoon(() => {
          this.destroy();
        });
        break;
    }
  },

  onMessage(event) {
    if (!event.data) {
      return;
    }
    // Toolbox document is still chrome and disallow identifying message
    // origin via event.source as it is null. So use a custom id.
    if (event.data.frameId != this.frameId) {
      return;
    }
    switch (event.data.name) {
      case "switch-host":
        this.switchHost(event.data.hostType);
        break;
      case "maximize-host":
        this.host.maximize();
        break;
      case "raise-host":
        this.host.raise();
        break;
      case "toggle-minimize-mode":
        this.host.toggleMinimizeMode(event.data.toolbarHeight);
        break;
      case "set-host-title":
        this.host.setTitle(event.data.title);
        break;
    }
  },

  postMessage(data) {
    let window = this.host.frame.contentWindow;
    window.postMessage(data, "*");
  },

  destroy() {
    this.destroyHost();
    this.host = null;
    this.hostType = null;
    this.target = null;
  },

  /**
   * Create a host object based on the given host type.
   *
   * Warning: bottom and sidebar hosts require that the toolbox target provides
   * a reference to the attached tab. Not all Targets have a tab property -
   * make sure you correctly mix and match hosts and targets.
   *
   * @param {string} hostType
   *        The host type of the new host object
   *
   * @return {Host} host
   *        The created host object
   */
  createHost(hostType, options) {
    if (!Hosts[hostType]) {
      throw new Error("Unknown hostType: " + hostType);
    }

    let newHost = new Hosts[hostType](this.target.tab, options);
    // Update the label and icon when the state changes.
    newHost.on("minimized", this.onHostMinimized);
    newHost.on("maximized", this.onHostMaximized);
    return newHost;
  },

  onHostMinimized() {
    this.postMessage({
      name: "host-minimized"
    });
  },

  onHostMaximized() {
    this.postMessage({
      name: "host-maximized"
    });
  },

  switchHost: Task.async(function* (hostType) {
    if (hostType == "previous") {
      // Switch to the last used host for the toolbox UI.
      // This is determined by the devtools.toolbox.previousHost pref.
      hostType = Services.prefs.getCharPref(PREVIOUS_HOST);

      // Handle the case where the previous host happens to match the current
      // host. If so, switch to bottom if it's not already used, and side if not.
      if (hostType === this.hostType) {
        if (hostType === Toolbox.HostType.BOTTOM) {
          hostType = Toolbox.HostType.SIDE;
        } else {
          hostType = Toolbox.HostType.BOTTOM;
        }
      }
    }
    let iframe = this.host.frame;
    let newHost = this.createHost(hostType);
    let newIframe = yield newHost.create();
    // change toolbox document's parent to the new host
    newIframe.swapFrameLoaders(iframe);

    this.destroyHost();

    if (this.hostType != Toolbox.HostType.CUSTOM) {
      Services.prefs.setCharPref(PREVIOUS_HOST, this.hostType);
    }

    this.host = newHost;
    this.hostType = hostType;
    this.host.setTitle(this.host.frame.contentWindow.document.title);
    this.host.frame.ownerDocument.defaultView.addEventListener("message", this);
    this.host.frame.addEventListener("unload", this, true);

    if (hostType != Toolbox.HostType.CUSTOM) {
      Services.prefs.setCharPref(LAST_HOST, hostType);
    }

    // Tell the toolbox the host changed
    this.postMessage({
      name: "switched-host",
      hostType
    });
  }),

  /**
   * Destroy the current host, and remove event listeners from its frame.
   *
   * @return {promise} to be resolved when the host is destroyed.
   */
  destroyHost() {
    // When Firefox toplevel is closed, the frame may already be detached and
    // the top level document gone
    if (this.host.frame.ownerDocument.defaultView) {
      this.host.frame.ownerDocument.defaultView.removeEventListener("message", this);
    }
    this.host.frame.removeEventListener("unload", this, true);

    this.host.off("minimized", this.onHostMinimized);
    this.host.off("maximized", this.onHostMaximized);
    return this.host.destroy();
  }
};
exports.ToolboxHostManager = ToolboxHostManager;
