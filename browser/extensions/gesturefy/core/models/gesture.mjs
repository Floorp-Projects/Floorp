import { isObject } from "/core/utils/commons.mjs";

import Command from "/core/models/command.mjs";

/**
 * This class represents a user defined gesture and provides easy access and manipulation methods
 * It is designed to allow easy conversation from and to JSON
 **/
export default class Gesture {
  constructor (pattern, command, label = "") {
    // if first argument is an object assume the gesture data is given in JSON
    if (arguments.length === 1 && isObject(arguments[0]) && arguments[0].hasOwnProperty("pattern") && arguments[0].hasOwnProperty("command")) {
      this._label = arguments[0].label || "";
      this._pattern = arguments[0].pattern;
      this._command = new Command(arguments[0].command);
    }
    else {
      if (!Array.isArray(pattern)) throw "The first argument must be an array.";
      if (!command instanceof Command) throw "The second argument must be an instance of the Command class.";
      if (typeof label !== "string") throw "The third argument must be of type string.";

      this._pattern = pattern;
      this._command = command;
      this._label = label;
    }
  }

  /**
   * Converts the class instance to a JavaScript object
   * This function is also automatically called when the JSON.stringify() option is invoked on an instance of this class
   **/
  toJSON () {
    const obj = {
      pattern: this._pattern,
      command: this._command.toJSON()
    };
    if (this._label) obj.label = this._label;
    return obj;
  }

  /**
   * Returns the gesture specific label if set, or the readable name of the command
   **/
  toString () {
    return this._label || this._command.toString();
  }

  getLabel () {
    return this._label;
  }

  setLabel (value) {
    if (typeof value !== "string") throw "The passed argument must be of type string.";
    this._label = value;
  }

  getPattern () {
    return this._pattern;
  }

  setPattern (value) {
    if (!Array.isArray(value)) throw "The passed argument must be an array.";
    this._pattern = value;
  }

  getCommand () {
    return this._command;
  }

  setCommand (value) {
    if (!value instanceof Command) throw "The passed argument must be an instance of the Command class.";
    this._command = value;
  }
}