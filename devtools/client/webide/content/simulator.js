/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cu = Components.utils;

const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { GetDevices, GetDeviceString } = require("devtools/client/shared/devices");
const { Services } = Cu.import("resource://gre/modules/Services.jsm");
const { Simulators, Simulator } = require("devtools/client/webide/modules/simulators");
const EventEmitter = require('devtools/shared/event-emitter');
const promise = require("promise");
const utils = require("devtools/client/webide/modules/utils");

const Strings = Services.strings.createBundle("chrome://devtools/locale/webide.properties");

var SimulatorEditor = {

  // Available Firefox OS Simulator addons (key: `addon.id`).
  _addons: {},

  // Available device simulation profiles (key: `device.name`).
  _devices: {},

  // The names of supported simulation options.
  _deviceOptions: [],

  // The <form> element used to edit Simulator options.
  _form: null,

  // The Simulator object being edited.
  _simulator: null,

  // Generate the dynamic form elements.
  init() {
    let promises = [];

    // Grab the <form> element.
    let form = this._form;
    if (!form) {
      // This is the first time we run `init()`, bootstrap some things.
      form = this._form = document.querySelector("#simulator-editor");
      form.addEventListener("change", this.update.bind(this));
      Simulators.on("configure", (e, simulator) => { this.edit(simulator) });
      // Extract the list of device simulation options we'll support.
      let deviceFields = form.querySelectorAll("*[data-device]");
      this._deviceOptions = [].map.call(deviceFields, field => field.name);
    }

    // Append a new <option> to a <select> (or <optgroup>) element.
    function opt(select, value, text) {
      let option = document.createElement("option");
      option.value = value;
      option.textContent = text;
      select.appendChild(option);
    }

    // Generate B2G version selector.
    promises.push(Simulators.findSimulatorAddons().then(addons => {
      this._addons = {};
      form.version.innerHTML = "";
      form.version.classList.remove("custom");
      addons.forEach(addon => {
        this._addons[addon.id] = addon;
        opt(form.version, addon.id, addon.name);
      });
      opt(form.version, "custom", "");
      opt(form.version, "pick", Strings.GetStringFromName("simulator_custom_binary"));
    }));

    // Generate profile selector.
    form.profile.innerHTML = "";
    form.profile.classList.remove("custom");
    opt(form.profile, "default", Strings.GetStringFromName("simulator_default_profile"));
    opt(form.profile, "custom", "");
    opt(form.profile, "pick", Strings.GetStringFromName("simulator_custom_profile"));

    // Generate example devices list.
    form.device.innerHTML = "";
    form.device.classList.remove("custom");
    opt(form.device, "custom", Strings.GetStringFromName("simulator_custom_device"));
    promises.push(GetDevices().then(devices => {
      devices.TYPES.forEach(type => {
        let b2gDevices = devices[type].filter(d => d.firefoxOS);
        if (b2gDevices.length < 1) {
          return;
        }
        let optgroup = document.createElement("optgroup");
        optgroup.label = GetDeviceString(type);
        b2gDevices.forEach(device => {
          this._devices[device.name] = device;
          opt(optgroup, device.name, device.name);
        });
        form.device.appendChild(optgroup);
      });
    }));

    return promise.all(promises);
  },

  // Edit the configuration of an existing Simulator, or create a new one.
  edit(simulator) {
    // If no Simulator was given to edit, we're creating a new one.
    if (!simulator) {
      simulator = new Simulator(); // Default options.
      Simulators.add(simulator);
    }

    this._simulator = null;

    return this.init().then(() => {
      this._simulator = simulator;

      // Update the form fields.
      this._form.name.value = simulator.name;
      this.updateVersionSelector();
      this.updateProfileSelector();
      this.updateDeviceSelector();
      this.updateDeviceFields();
    });
  },

  // Close the configuration panel.
  close() {
    this._simulator = null;
    window.parent.UI.openProject();
  },

  // Restore the simulator to its default configuration.
  restoreDefaults() {
    let simulator = this._simulator;
    this.version = simulator.addon.id;
    this.profile = "default";
    simulator.restoreDefaults();
    Simulators.emitUpdated();
    return this.edit(simulator);
  },

  // Delete this simulator.
  deleteSimulator() {
    Simulators.remove(this._simulator);
    this.close();
  },

  // Select an available option, or set the "custom" option.
  updateSelector(selector, value) {
    selector.value = value;
    if (selector[selector.selectedIndex].value !== value) {
      selector.value = "custom";
      selector.classList.add("custom");
      selector[selector.selectedIndex].textContent = value;
    }
  },

  // VERSION: Can be an installed `addon.id` or a custom binary path.

  get version() {
    return this._simulator.options.b2gBinary || this._simulator.addon.id;
  },

  set version(value) {
    let form = this._form;
    let simulator = this._simulator;
    let oldVer = simulator.version;
    if (this._addons[value]) {
      // `value` is a simulator addon ID.
      simulator.addon = this._addons[value];
      simulator.options.b2gBinary = null;
    } else {
      // `value` is a custom binary path.
      simulator.options.b2gBinary = value;
      // TODO (Bug 1146531) Indicate that a custom profile is now required.
    }
    // If `form.name` contains the old version, update its last occurrence.
    if (form.name.value.contains(oldVer) && simulator.version !== oldVer) {
      let regex = new RegExp("(.*)" + oldVer);
      let name = form.name.value.replace(regex, "$1" + simulator.version);
      simulator.options.name = form.name.value = Simulators.uniqueName(name);
    }
  },

  updateVersionSelector() {
    this.updateSelector(this._form.version, this.version);
  },

  // PROFILE. Can be "default" or a custom profile directory path.

  get profile() {
    return this._simulator.options.gaiaProfile || "default";
  },

  set profile(value) {
    this._simulator.options.gaiaProfile = (value == "default" ? null : value);
  },

  updateProfileSelector() {
    this.updateSelector(this._form.profile, this.profile);
  },

  // DEVICE. Can be an existing `device.name` or "custom".

  get device() {
    let devices = this._devices;
    let simulator = this._simulator;

    // Search for the name of a device matching current simulator options.
    for (let name in devices) {
      let match = true;
      for (let option of this._deviceOptions) {
        if (simulator.options[option] === devices[name][option]) {
          continue;
        }
        match = false;
        break;
      }
      if (match) {
        return name;
      }
    }
    return "custom";
  },

  set device(name) {
    let device = this._devices[name];
    if (!device) {
      return;
    }
    let form = this._form;
    let simulator = this._simulator;
    this._deviceOptions.forEach(option => {
      simulator.options[option] = form[option].value = device[option] || null;
    });
    // TODO (Bug 1146531) Indicate when a custom profile is required (e.g. for
    // tablet, TV…).
  },

  updateDeviceSelector() {
    this.updateSelector(this._form.device, this.device);
  },

  // Erase any current values, trust only the `simulator.options`.
  updateDeviceFields() {
    let form = this._form;
    let simulator = this._simulator;
    this._deviceOptions.forEach(option => {
      form[option].value = simulator.options[option];
    });
  },

  // Handle a change in our form's fields.
  update(event) {
    let simulator = this._simulator;
    if (!simulator) {
      return;
    }
    let form = this._form;
    let input = event.target;
    switch (input.name) {
      case "name":
        simulator.options.name = input.value;
        break;
      case "version":
        switch (input.value) {
          case "pick":
            let file = utils.getCustomBinary(window);
            if (file) {
              this.version = file.path;
            }
            // Whatever happens, don't stay on the "pick" option.
            this.updateVersionSelector();
            break;
          case "custom":
            this.version = input[input.selectedIndex].textContent;
            break;
          default:
            this.version = input.value;
        }
        break;
      case "profile":
        switch (input.value) {
          case "pick":
            let directory = utils.getCustomProfile(window);
            if (directory) {
              this.profile = directory.path;
            }
            // Whatever happens, don't stay on the "pick" option.
            this.updateProfileSelector();
            break;
          case "custom":
            this.profile = input[input.selectedIndex].textContent;
            break;
          default:
            this.profile = input.value;
        }
        break;
      case "device":
        this.device = input.value;
        break;
      default:
        simulator.options[input.name] = input.value || null;
        this.updateDeviceSelector();
    }
    Simulators.emitUpdated();
  },
};

window.addEventListener("load", function onLoad() {
  document.querySelector("#close").onclick = e => {
    SimulatorEditor.close();
  };
  document.querySelector("#reset").onclick = e => {
    SimulatorEditor.restoreDefaults();
  };
  document.querySelector("#remove").onclick = e => {
    SimulatorEditor.deleteSimulator();
  };

  // We just loaded, so we probably missed the first configure request.
  SimulatorEditor.edit(Simulators._lastConfiguredSimulator);
});
