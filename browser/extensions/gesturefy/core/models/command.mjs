import { isObject } from "/core/utils/commons.mjs";

import * as Commands from "/core/commands.mjs";

/**
 * This class represents a user defined command and provides easy access and manipulation methods
 * It is designed to allow easy conversation from and to JSON
 * The execute method calls the corresponding function
 **/
export default class Command {

  /**
   * The constructor can be passed a command name (string) and optionally a settings object
   * Alternatively only a JSON formatted command object can be passed containing the keys: name, settings
   **/
  constructor (name, settings = null) {
    let settingsPairs = []
    // if first argument is an object assume the command data is given in JSON
    if (arguments.length === 1 && isObject(arguments[0]) && arguments[0].hasOwnProperty("name")) {
      this._name = arguments[0].name;
      if (arguments[0].hasOwnProperty("settings")) {
        // convert object to key value pairs
        settingsPairs.push(...Object.entries(arguments[0].settings));
      }
    }
    else {
      if (typeof name !== "string") throw "The first argument must be of type string.";
      this._name = name;
      if (settings) {
        if (!isObject(settings)) throw "The second argument must be an object.";
        // convert object to key value pairs
        settingsPairs.push(...Object.entries(settings));
      }
    }
    // throw error if command function does not exist
    if (!this._name in Commands) throw "There exists no corresponding function for the passed command name.";
    // store settings as map
    this._settings = new Map(settingsPairs);
  }

  /**
   * Converts the class instance to a JavaScript object
   * This function is also automatically called when the JSON.stringify() option is invoked on an instance of this class
   **/
  toJSON () {
    const obj = { name: this._name };
    if (this._settings.size > 0) obj.settings = Object.fromEntries(this._settings);
    return obj;
  }

  /**
   * Returns the actual readable name of the command
   **/
  toString () {
    return browser.i18n.getMessage(`commandLabel${this.getName()}`);
  }

  /**
   * Executes the corresponding command function
   * The command instance is set as the execution context (this value) so the command can access its methods (and therefore settings)
   * Passes the sender and source data objects as the function arguments
   * This function returns the return value of the command function (all command functions return a promise)
   **/
  execute (sender, data) {
    if (!isObject(sender)) throw "The first argument must be an object.";
    if (!isObject(data)) throw "The second argument must be an object.";
    return Commands[this._name].call(
      this,
      sender,
      data
    );
  }

  getName () {
    return this._name;
  }

  setName (value) {
    if (typeof value !== "string") throw "The passed argument must be of type string.";
    this._name = value;
  }

  getSetting (setting) {
    if (typeof setting !== "string") throw "The passed argument must be of type string.";
    return this._settings.get(setting);
  }

  setSetting (setting, value) {
    if (typeof setting !== "string") throw "The first argument must be of type string.";
    this._settings.set(setting, value);
  }

  hasSetting (setting) {
    if (typeof setting !== "string") throw "The passed argument must be of type string.";
    return this._settings.has(setting);
  }

  deleteSetting (setting) {
    if (typeof setting !== "string") throw "The passed argument must be of type string.";
    return this._settings.delete(setting);
  }

  hasSettings () {
    return this._settings.size > 0;
  }

  clearSettings () {
    return this._settings.clear();
  }
}