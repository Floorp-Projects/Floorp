/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

var util = require('../util/util');
var l10n = require('../util/l10n');

/**
 * Implement the localization algorithm for any documentation objects (i.e.
 * description and manual) in a command.
 * @param data The data assigned to a description or manual property
 * @param onUndefined If data == null, should we return the data untouched or
 * lookup a 'we don't know' key in it's place.
 */
function lookup(data, onUndefined) {
  if (data == null) {
    if (onUndefined) {
      return l10n.lookup(onUndefined);
    }

    return data;
  }

  if (typeof data === 'string') {
    return data;
  }

  if (typeof data === 'object') {
    if (data.key) {
      return l10n.lookup(data.key);
    }

    var locales = l10n.getPreferredLocales();
    var translated;
    locales.some(function(locale) {
      translated = data[locale];
      return translated != null;
    });
    if (translated != null) {
      return translated;
    }

    console.error('Can\'t find locale in descriptions: ' +
            'locales=' + JSON.stringify(locales) + ', ' +
            'description=' + JSON.stringify(data));
    return '(No description)';
  }

  return l10n.lookup(onUndefined);
}


/**
 * The command object is mostly just setup around a commandSpec (as passed to
 * Commands.add()).
 */
function Command(types, commandSpec) {
  Object.keys(commandSpec).forEach(function(key) {
    this[key] = commandSpec[key];
  }, this);

  if (!this.name) {
    throw new Error('All registered commands must have a name');
  }

  if (this.params == null) {
    this.params = [];
  }
  if (!Array.isArray(this.params)) {
    throw new Error('command.params must be an array in ' + this.name);
  }

  this.hasNamedParameters = false;
  this.description = 'description' in this ? this.description : undefined;
  this.description = lookup(this.description, 'canonDescNone');
  this.manual = 'manual' in this ? this.manual : undefined;
  this.manual = lookup(this.manual);

  // At this point this.params has nested param groups. We want to flatten it
  // out and replace the param object literals with Parameter objects
  var paramSpecs = this.params;
  this.params = [];
  this.paramGroups = {};
  this._shortParams = {};

  var addParam = param => {
    var groupName = param.groupName || l10n.lookup('canonDefaultGroupName');
    this.params.push(param);
    if (!this.paramGroups.hasOwnProperty(groupName)) {
      this.paramGroups[groupName] = [];
    }
    this.paramGroups[groupName].push(param);
  };

  // Track if the user is trying to mix default params and param groups.
  // All the non-grouped parameters must come before all the param groups
  // because non-grouped parameters can be assigned positionally, so their
  // index is important. We don't want 'holes' in the order caused by
  // parameter groups.
  var usingGroups = false;

  // In theory this could easily be made recursive, so param groups could
  // contain nested param groups. Current thinking is that the added
  // complexity for the UI probably isn't worth it, so this implementation
  // prevents nesting.
  paramSpecs.forEach(function(spec) {
    if (!spec.group) {
      var param = new Parameter(types, spec, this, null);
      addParam(param);

      if (!param.isPositionalAllowed) {
        this.hasNamedParameters = true;
      }

      if (usingGroups && param.groupName == null) {
        throw new Error('Parameters can\'t come after param groups.' +
                        ' Ignoring ' + this.name + '/' + spec.name);
      }

      if (param.groupName != null) {
        usingGroups = true;
      }
    }
    else {
      spec.params.forEach(function(ispec) {
        var param = new Parameter(types, ispec, this, spec.group);
        addParam(param);

        if (!param.isPositionalAllowed) {
          this.hasNamedParameters = true;
        }
      }, this);

      usingGroups = true;
    }
  }, this);

  this.params.forEach(function(param) {
    if (param.short != null) {
      if (this._shortParams[param.short] != null) {
        throw new Error('Multiple params using short name ' + param.short);
      }
      this._shortParams[param.short] = param;
    }
  }, this);
}

/**
 * JSON serializer that avoids non-serializable data
 * @param customProps Array of strings containing additional properties which,
 * if specified in the command spec, will be included in the JSON. Normally we
 * transfer only the properties required for GCLI to function.
 */
Command.prototype.toJson = function(customProps) {
  var json = {
    item: 'command',
    name: this.name,
    params: this.params.map(function(param) { return param.toJson(); }),
    returnType: this.returnType,
    isParent: (this.exec == null)
  };

  if (this.description !==  l10n.lookup('canonDescNone')) {
    json.description = this.description;
  }
  if (this.manual != null) {
    json.manual = this.manual;
  }
  if (this.hidden != null) {
    json.hidden = this.hidden;
  }

  if (Array.isArray(customProps)) {
    customProps.forEach(prop => {
      if (this[prop] != null) {
        json[prop] = this[prop];
      }
    });
  }

  return json;
};

/**
 * Easy way to lookup parameters by full name
 */
Command.prototype.getParameterByName = function(name) {
  var reply;
  this.params.forEach(function(param) {
    if (param.name === name) {
      reply = param;
    }
  });
  return reply;
};

/**
 * Easy way to lookup parameters by short name
 */
Command.prototype.getParameterByShortName = function(short) {
  return this._shortParams[short];
};

exports.Command = Command;


/**
 * A wrapper for a paramSpec so we can sort out shortened versions names for
 * option switches
 */
function Parameter(types, paramSpec, command, groupName) {
  this.command = command || { name: 'unnamed' };
  this.paramSpec = paramSpec;
  this.name = this.paramSpec.name;
  this.type = this.paramSpec.type;
  this.short = this.paramSpec.short;

  if (this.short != null && !/[0-9A-Za-z]/.test(this.short)) {
    throw new Error('\'short\' value must be a single alphanumeric digit.');
  }

  this.groupName = groupName;
  if (this.groupName != null) {
    if (this.paramSpec.option != null) {
      throw new Error('Can\'t have a "option" property in a nested parameter');
    }
  }
  else {
    if (this.paramSpec.option != null) {
      this.groupName = (this.paramSpec.option === true) ?
                       l10n.lookup('canonDefaultGroupName') :
                       '' + this.paramSpec.option;
    }
  }

  if (!this.name) {
    throw new Error('In ' + this.command.name +
                    ': all params must have a name');
  }

  var typeSpec = this.type;
  this.type = types.createType(typeSpec);
  if (this.type == null) {
    console.error('Known types: ' + types.getTypeNames().join(', '));
    throw new Error('In ' + this.command.name + '/' + this.name +
                    ': can\'t find type for: ' + JSON.stringify(typeSpec));
  }

  // boolean parameters have an implicit defaultValue:false, which should
  // not be changed. See the docs.
  if (this.type.name === 'boolean' &&
      this.paramSpec.defaultValue !== undefined) {
    throw new Error('In ' + this.command.name + '/' + this.name +
                    ': boolean parameters can not have a defaultValue.' +
                    ' Ignoring');
  }

  // All parameters that can only be set via a named parameter must have a
  // non-undefined default value
  if (!this.isPositionalAllowed && this.paramSpec.defaultValue === undefined &&
      this.type.getBlank == null && this.type.name !== 'boolean') {
    throw new Error('In ' + this.command.name + '/' + this.name +
                    ': Missing defaultValue for optional parameter.');
  }

  if (this.paramSpec.defaultValue !== undefined) {
    this.defaultValue = this.paramSpec.defaultValue;
  }
  else {
    Object.defineProperty(this, 'defaultValue', {
      get: function() {
        return this.type.getBlank().value;
      },
      enumerable: true
    });
  }

  // Resolve the documentation
  this.manual = lookup(this.paramSpec.manual);
  this.description = lookup(this.paramSpec.description, 'canonDescNone');

  // Is the user required to enter data for this parameter? (i.e. has
  // defaultValue been set to something other than undefined)
  // TODO: When the defaultValue comes from type.getBlank().value (see above)
  // then perhaps we should set using something like
  //   isDataRequired = (type.getBlank().status !== VALID)
  this.isDataRequired = (this.defaultValue === undefined);

  // Are we allowed to assign data to this parameter using positional
  // parameters?
  this.isPositionalAllowed = this.groupName == null;
}

/**
 * Does the given name uniquely identify this param (among the other params
 * in this command)
 * @param name The name to check
 */
Parameter.prototype.isKnownAs = function(name) {
  return (name === '--' + this.name) || (name === '-' + this.short);
};

/**
 * Reflect the paramSpec 'hidden' property (dynamically so it can change)
 */
Object.defineProperty(Parameter.prototype, 'hidden', {
  get: function() {
    return this.paramSpec.hidden;
  },
  enumerable: true
});

/**
 * JSON serializer that avoids non-serializable data
 */
Parameter.prototype.toJson = function() {
  var json = {
    name: this.name,
    type: this.type.getSpec(this.command.name, this.name),
    short: this.short
  };

  // Values do not need to be serializable, so we don't try. For the client
  // side (which doesn't do any executing) we only care whether default value is
  // undefined, null, or something else.
  if (this.paramSpec.defaultValue !== undefined) {
    json.defaultValue = (this.paramSpec.defaultValue === null) ? null : {};
  }
  if (this.paramSpec.description != null) {
    json.description = this.paramSpec.description;
  }
  if (this.paramSpec.manual != null) {
    json.manual = this.paramSpec.manual;
  }
  if (this.paramSpec.hidden != null) {
    json.hidden = this.paramSpec.hidden;
  }

  // groupName can be set outside a paramSpec, (e.g. in grouped parameters)
  // but it works like 'option' does so we use 'option' for groupNames
  if (this.groupName != null || this.paramSpec.option != null) {
    json.option = this.groupName || this.paramSpec.option;
  }

  return json;
};

exports.Parameter = Parameter;


/**
 * A store for a list of commands
 * @param types Each command uses a set of Types to parse its parameters so the
 * Commands container needs access to the list of available types.
 * @param location String that, if set will force all commands to have a
 * matching runAt property to be accepted
 */
function Commands(types, location) {
  this.types = types;
  this.location = location;

  // A lookup hash of our registered commands
  this._commands = {};
  // A sorted list of command names, we regularly want them in order, so pre-sort
  this._commandNames = [];
  // A lookup of the original commandSpecs by command name
  this._commandSpecs = {};

  // Enable people to be notified of changes to the list of commands
  this.onCommandsChange = util.createEvent('commands.onCommandsChange');
}

/**
 * Add a command to the list of known commands.
 * @param commandSpec The command and its metadata.
 * @return The new command, or null if a location property has been set and the
 * commandSpec doesn't have a matching runAt property.
 */
Commands.prototype.add = function(commandSpec) {
  if (this.location != null && commandSpec.runAt != null &&
      commandSpec.runAt !== this.location) {
    return;
  }

  if (this._commands[commandSpec.name] != null) {
    // Roughly commands.remove() without the event call, which we do later
    delete this._commands[commandSpec.name];
    this._commandNames = this._commandNames.filter(function(test) {
      return test !== commandSpec.name;
    });
  }

  var command = new Command(this.types, commandSpec);
  this._commands[commandSpec.name] = command;
  this._commandNames.push(commandSpec.name);
  this._commandNames.sort();

  this._commandSpecs[commandSpec.name] = commandSpec;

  this.onCommandsChange();
  return command;
};

/**
 * Remove an individual command. The opposite of Commands.add().
 * Removing a non-existent command is a no-op.
 * @param commandOrName Either a command name or the command itself.
 * @return true if a command was removed, false otherwise.
 */
Commands.prototype.remove = function(commandOrName) {
  var name = typeof commandOrName === 'string' ?
          commandOrName :
          commandOrName.name;

  if (!this._commands[name]) {
    return false;
  }

  // See start of commands.add if changing this code
  delete this._commands[name];
  delete this._commandSpecs[name];
  this._commandNames = this._commandNames.filter(function(test) {
    return test !== name;
  });

  this.onCommandsChange();
  return true;
};

/**
 * Retrieve a command by name
 * @param name The name of the command to retrieve
 */
Commands.prototype.get = function(name) {
  // '|| undefined' is to silence 'reference to undefined property' warnings
  return this._commands[name] || undefined;
};

/**
 * Get an array of all the registered commands.
 */
Commands.prototype.getAll = function() {
  return Object.keys(this._commands).map(function(name) {
    return this._commands[name];
  }, this);
};

/**
 * Get access to the stored commandMetaDatas (i.e. before they were made into
 * instances of Command/Parameters) so we can remote them.
 * @param customProps Array of strings containing additional properties which,
 * if specified in the command spec, will be included in the JSON. Normally we
 * transfer only the properties required for GCLI to function.
 */
Commands.prototype.getCommandSpecs = function(customProps) {
  var commandSpecs = [];

  Object.keys(this._commands).forEach(name => {
    var command = this._commands[name];
    if (!command.noRemote) {
      commandSpecs.push(command.toJson(customProps));
    }
  });

  return commandSpecs;
};

/**
 * Add a set of commands that are executed somewhere else, optionally with a
 * command prefix to distinguish these commands from a local set of commands.
 * @param commandSpecs Presumably as obtained from getCommandSpecs
 * @param remoter Function to call on exec of a new remote command. This is
 * defined just like an exec function (i.e. that takes args/context as params
 * and returns a promise) with one extra feature, that the context includes a
 * 'commandName' property that contains the original command name.
 * @param prefix The name prefix that we assign to all command names
 * @param to URL-like string that describes where the commands are executed.
 * This is to complete the parent command description.
 */
Commands.prototype.addProxyCommands = function(commandSpecs, remoter, prefix, to) {
  if (prefix != null) {
    if (this._commands[prefix] != null) {
      throw new Error(l10n.lookupFormat('canonProxyExists', [ prefix ]));
    }

    // We need to add the parent command so all the commands from the other
    // system have a parent
    this.add({
      name: prefix,
      isProxy: true,
      description: l10n.lookupFormat('canonProxyDesc', [ to ]),
      manual: l10n.lookupFormat('canonProxyManual', [ to ])
    });
  }

  commandSpecs.forEach(commandSpec => {
    var originalName = commandSpec.name;
    if (!commandSpec.isParent) {
      commandSpec.exec = (args, context) => {
        context.commandName = originalName;
        return remoter(args, context);
      };
    }

    if (prefix != null) {
      commandSpec.name = prefix + ' ' + commandSpec.name;
    }
    commandSpec.isProxy = true;
    this.add(commandSpec);
  });
};

/**
 * Remove a set of commands added with addProxyCommands.
 * @param prefix The name prefix that we assign to all command names
 */
Commands.prototype.removeProxyCommands = function(prefix) {
  var toRemove = [];
  Object.keys(this._commandSpecs).forEach(name => {
    if (name.indexOf(prefix) === 0) {
      toRemove.push(name);
    }
  });

  var removed = [];
  toRemove.forEach(name => {
    var command = this.get(name);
    if (command.isProxy) {
      this.remove(name);
      removed.push(name);
    }
    else {
      console.error('Skipping removal of \'' + name +
                    '\' because it is not a proxy command.');
    }
  });

  return removed;
};

exports.Commands = Commands;

/**
 * CommandOutputManager stores the output objects generated by executed
 * commands.
 *
 * CommandOutputManager is exposed to the the outside world and could (but
 * shouldn't) be used before gcli.startup() has been called.
 * This could should be defensive to that where possible, and we should
 * certainly document if the use of it or similar will fail if used too soon.
 */
function CommandOutputManager() {
  this.onOutput = util.createEvent('CommandOutputManager.onOutput');
}

exports.CommandOutputManager = CommandOutputManager;
