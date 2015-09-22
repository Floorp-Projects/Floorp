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

var util = require('./util/util');
var host = require('./util/host');
var l10n = require('./util/l10n');

var view = require('./ui/view');
var Parameter = require('./commands/commands').Parameter;
var CommandOutputManager = require('./commands/commands').CommandOutputManager;

var Status = require('./types/types').Status;
var Conversion = require('./types/types').Conversion;
var commandModule = require('./types/command');
var selectionModule = require('./types/selection');

var Argument = require('./types/types').Argument;
var ArrayArgument = require('./types/types').ArrayArgument;
var NamedArgument = require('./types/types').NamedArgument;
var TrueNamedArgument = require('./types/types').TrueNamedArgument;
var MergedArgument = require('./types/types').MergedArgument;
var ScriptArgument = require('./types/types').ScriptArgument;

var RESOLVED = Promise.resolve(undefined);

// Helper to produce a `deferred` object
// using DOM Promise
function defer() {
  let resolve, reject;
  let p = new Promise((a, b) => {
    resolve = a;
    reject = b;
  });
  return {
    promise: p,
    resolve: resolve,
    reject: reject
  };
}

/**
 * This is a list of the known command line components to enable certain
 * privileged commands to alter parts of a running command line. It is an array
 * of objects shaped like:
 *   { conversionContext:..., executionContext:..., mapping:... }
 * So lookup is O(n) where 'n' is the number of command lines.
 */
var instances = [];

/**
 * An indexOf that looks-up both types of context
 */
function instanceIndex(context) {
  for (var i = 0; i < instances.length; i++) {
    var instance = instances[i];
    if (instance.conversionContext === context ||
        instance.executionContext === context) {
      return i;
    }
  }
  return -1;
}

/**
 * findInstance gets access to a Terminal object given a conversionContext or
 * an executionContext (it doesn't have to be a terminal object, just whatever
 * was passed into addMapping()
 */
exports.getMapping = function(context) {
  var index = instanceIndex(context);
  if (index === -1) {
    console.log('Missing mapping for context: ', context);
    console.log('Known contexts: ', instances);
    throw new Error('Missing mapping for context');
  }
  return instances[index].mapping;
};

/**
 * Add a requisition context->terminal mapping
 */
var addMapping = function(requisition) {
  if (instanceIndex(requisition.conversionContext) !== -1) {
    throw new Error('Remote existing mapping before adding a new one');
  }

  instances.push({
    conversionContext: requisition.conversionContext,
    executionContext: requisition.executionContext,
    mapping: { requisition: requisition }
  });
};

/**
 * Remove a requisition context->terminal mapping
 */
var removeMapping = function(requisition) {
  var index = instanceIndex(requisition.conversionContext);
  instances.splice(index, 1);
};

/**
 * Assignment is a link between a parameter and the data for that parameter.
 * The data for the parameter is available as in the preferred type and as
 * an Argument for the CLI.
 * <p>We also record validity information where applicable.
 * <p>For values, null and undefined have distinct definitions. null means
 * that a value has been provided, undefined means that it has not.
 * Thus, null is a valid default value, and common because it identifies an
 * parameter that is optional. undefined means there is no value from
 * the command line.
 * @constructor
 */
function Assignment(param) {
  // The parameter that we are assigning to
  this.param = param;
  this.conversion = undefined;
}

/**
 * Easy accessor for conversion.arg.
 * This is a read-only property because writes to arg should be done through
 * the 'conversion' property.
 */
Object.defineProperty(Assignment.prototype, 'arg', {
  get: function() {
    return this.conversion == null ? undefined : this.conversion.arg;
  },
  enumerable: true
});

/**
 * Easy accessor for conversion.value.
 * This is a read-only property because writes to value should be done through
 * the 'conversion' property.
 */
Object.defineProperty(Assignment.prototype, 'value', {
  get: function() {
    return this.conversion == null ? undefined : this.conversion.value;
  },
  enumerable: true
});

/**
 * Easy (and safe) accessor for conversion.message
 */
Object.defineProperty(Assignment.prototype, 'message', {
  get: function() {
    if (this.conversion != null && this.conversion.message) {
      return this.conversion.message;
    }
    // ERROR conversions have messages, VALID conversions don't need one, so
    // we just need to consider INCOMPLETE conversions.
    if (this.getStatus() === Status.INCOMPLETE) {
      return l10n.lookupFormat('cliIncompleteParam', [ this.param.name ]);
    }
    return '';
  },
  enumerable: true
});

/**
 * Easy (and safe) accessor for conversion.getPredictions()
 * @return An array of objects with name and value elements. For example:
 * [ { name:'bestmatch', value:foo1 }, { name:'next', value:foo2 }, ... ]
 */
Assignment.prototype.getPredictions = function(context) {
  return this.conversion == null ? [] : this.conversion.getPredictions(context);
};

/**
 * Accessor for a prediction by index.
 * This is useful above <tt>getPredictions()[index]</tt> because it normalizes
 * index to be within the bounds of the predictions, which means that the UI
 * can maintain an index of which prediction to choose without caring how many
 * predictions there are.
 * @param rank The index of the prediction to choose
 */
Assignment.prototype.getPredictionRanked = function(context, rank) {
  if (rank == null) {
    rank = 0;
  }

  if (this.isInName()) {
    return Promise.resolve(undefined);
  }

  return this.getPredictions(context).then(function(predictions) {
    if (predictions.length === 0) {
      return undefined;
    }

    rank = rank % predictions.length;
    if (rank < 0) {
      rank = predictions.length + rank;
    }
    return predictions[rank];
  }.bind(this));
};

/**
 * Some places want to take special action if we are in the name part of a
 * named argument (i.e. the '--foo' bit).
 * Currently this does not take actual cursor position into account, it just
 * assumes that the cursor is at the end. In the future we will probably want
 * to take this into account.
 */
Assignment.prototype.isInName = function() {
  return this.conversion.arg.type === 'NamedArgument' &&
         this.conversion.arg.prefix.slice(-1) !== ' ';
};

/**
 * Work out what the status of the current conversion is which involves looking
 * not only at the conversion, but also checking if data has been provided
 * where it should.
 * @param arg For assignments with multiple args (e.g. array assignments) we
 * can narrow the search for status to a single argument.
 */
Assignment.prototype.getStatus = function(arg) {
  if (this.param.isDataRequired && !this.conversion.isDataProvided()) {
    return Status.INCOMPLETE;
  }

  // Selection/Boolean types with a defined range of values will say that
  // '' is INCOMPLETE, but the parameter may be optional, so we don't ask
  // if the user doesn't need to enter something and hasn't done so.
  if (!this.param.isDataRequired && this.arg.type === 'BlankArgument') {
    return Status.VALID;
  }

  return this.conversion.getStatus(arg);
};

/**
 * Helper when we're rebuilding command lines.
 */
Assignment.prototype.toString = function() {
  return this.conversion.toString();
};

/**
 * For test/debug use only. The output from this function is subject to wanton
 * random change without notice, and should not be relied upon to even exist
 * at some later date.
 */
Object.defineProperty(Assignment.prototype, '_summaryJson', {
  get: function() {
    return {
      param: this.param.name + '/' + this.param.type.name,
      defaultValue: this.param.defaultValue,
      arg: this.conversion.arg._summaryJson,
      value: this.value,
      message: this.message,
      status: this.getStatus().toString()
    };
  },
  enumerable: true
});

exports.Assignment = Assignment;


/**
 * How to dynamically execute JavaScript code
 */
var customEval = eval;

/**
 * Setup a function to be called in place of 'eval', generally for security
 * reasons
 */
exports.setEvalFunction = function(newCustomEval) {
  customEval = newCustomEval;
};

/**
 * Remove the binding done by setEvalFunction().
 * We purposely set customEval to undefined rather than to 'eval' because there
 * is an implication of setEvalFunction that we're in a security sensitive
 * situation. What if we can trick GCLI into calling unsetEvalFunction() at the
 * wrong time?
 * So to properly undo the effects of setEvalFunction(), you need to call
 * setEvalFunction(eval) rather than unsetEvalFunction(), however the latter is
 * preferred in most cases.
 */
exports.unsetEvalFunction = function() {
  customEval = undefined;
};

/**
 * 'eval' command
 */
var evalCmd = {
  item: 'command',
  name: '{',
  params: [
    {
      name: 'javascript',
      type: 'javascript',
      description: ''
    }
  ],
  hidden: true,
  description: { key: 'cliEvalJavascript' },
  exec: function(args, context) {
    var reply = customEval(args.javascript);
    return context.typedData(typeof reply, reply);
  },
  isCommandRegexp: /^\s*\{\s*/
};

exports.items = [ evalCmd ];

/**
 * This is a special assignment to reflect the command itself.
 */
function CommandAssignment(requisition) {
  var commandParamMetadata = {
    name: '__command',
    type: { name: 'command', allowNonExec: false }
  };
  // This is a hack so that rather than reply with a generic description of the
  // command assignment, we reply with the description of the assigned command,
  // (using a generic term if there is no assigned command)
  var self = this;
  Object.defineProperty(commandParamMetadata, 'description', {
    get: function() {
      var value = self.value;
      return value && value.description ?
          value.description :
          'The command to execute';
    },
    enumerable: true
  });
  this.param = new Parameter(requisition.system.types, commandParamMetadata);
}

CommandAssignment.prototype = Object.create(Assignment.prototype);

CommandAssignment.prototype.getStatus = function(arg) {
  return Status.combine(
    Assignment.prototype.getStatus.call(this, arg),
    this.conversion.value && this.conversion.value.exec ?
            Status.VALID : Status.INCOMPLETE
  );
};

exports.CommandAssignment = CommandAssignment;


/**
 * Special assignment used when ignoring parameters that don't have a home
 */
function UnassignedAssignment(requisition, arg) {
  var isIncompleteName = (arg.text.charAt(0) === '-');
  this.param = new Parameter(requisition.system.types, {
    name: '__unassigned',
    description: l10n.lookup('cliOptions'),
    type: {
      name: 'param',
      requisition: requisition,
      isIncompleteName: isIncompleteName
    }
  });

  // It would be nice to do 'conversion = parm.type.parse(arg, ...)' except
  // that type.parse returns a promise (even though it's synchronous in this
  // case)
  if (isIncompleteName) {
    var lookup = commandModule.getDisplayedParamLookup(requisition);
    var predictions = selectionModule.findPredictions(arg, lookup);
    this.conversion = selectionModule.convertPredictions(arg, predictions);
  }
  else {
    var message = l10n.lookup('cliUnusedArg');
    this.conversion = new Conversion(undefined, arg, Status.ERROR, message);
  }

  this.conversion.assignment = this;
}

UnassignedAssignment.prototype = Object.create(Assignment.prototype);

UnassignedAssignment.prototype.getStatus = function(arg) {
  return this.conversion.getStatus();
};

var logErrors = true;

/**
 * Allow tests that expect failures to avoid clogging up the console
 */
Object.defineProperty(exports, 'logErrors', {
  get: function() {
    return logErrors;
  },
  set: function(val) {
    logErrors = val;
  },
  enumerable: true
});

/**
 * A Requisition collects the information needed to execute a command.
 *
 * (For a definition of the term, see http://en.wikipedia.org/wiki/Requisition)
 * This term is used because carries the notion of a work-flow, or process to
 * getting the information to execute a command correct.
 * There is little point in a requisition for parameter-less commands because
 * there is no information to collect. A Requisition is a collection of
 * assignments of values to parameters, each handled by an instance of
 * Assignment.
 *
 * @param system Allows access to the various plug-in points in GCLI. At a
 * minimum it must contain commands and types objects.
 * @param options A set of options to customize how GCLI is used. Includes:
 * - environment An optional opaque object passed to commands in the
 *   Execution Context.
 * - document A DOM Document passed to commands using the Execution Context in
 *   order to allow creation of DOM nodes. If missing Requisition will use the
 *   global 'document', or leave undefined.
 * - commandOutputManager A custom commandOutputManager to which output should
 *   be sent
 * @constructor
 */
function Requisition(system, options) {
  options = options || {};

  this.environment = options.environment || {};
  this.document = options.document;
  if (this.document == null) {
    try {
      this.document = document;
    }
    catch (ex) {
      // Ignore
    }
  }

  this.commandOutputManager = options.commandOutputManager || new CommandOutputManager();
  this.system = system;

  this.shell = {
    cwd: '/', // Where we store the current working directory
    env: {}   // Where we store the current environment
  };

  // The command that we are about to execute.
  // @see setCommandConversion()
  this.commandAssignment = new CommandAssignment(this);

  // The object that stores of Assignment objects that we are filling out.
  // The Assignment objects are stored under their param.name for named
  // lookup. Note: We make use of the property of Javascript objects that
  // they are not just hashmaps, but linked-list hashmaps which iterate in
  // insertion order.
  // _assignments excludes the commandAssignment.
  this._assignments = {};

  // The count of assignments. Excludes the commandAssignment
  this.assignmentCount = 0;

  // Used to store cli arguments in the order entered on the cli
  this._args = [];

  // Used to store cli arguments that were not assigned to parameters
  this._unassigned = [];

  // Changes can be asynchronous, when one update starts before another
  // finishes we abandon the former change
  this._nextUpdateId = 0;

  // We can set a prefix to typed commands to make it easier to focus on
  // Allowing us to type "add -a; commit" in place of "git add -a; git commit"
  this.prefix = '';

  addMapping(this);
  this._setBlankAssignment(this.commandAssignment);

  // If a command calls context.update then the UI needs some way to be
  // informed of the change
  this.onExternalUpdate = util.createEvent('Requisition.onExternalUpdate');
}

/**
 * Avoid memory leaks
 */
Requisition.prototype.destroy = function() {
  this.document = undefined;
  this.environment = undefined;
  removeMapping(this);
};

/**
 * If we're about to make an asynchronous change when other async changes could
 * overtake this one, then we want to be able to bail out if overtaken. The
 * value passed back from beginChange should be passed to endChangeCheckOrder
 * on completion of calculation, before the results are applied in order to
 * check that the calculation has not been overtaken
 */
Requisition.prototype._beginChange = function() {
  var updateId = this._nextUpdateId;
  this._nextUpdateId++;
  return updateId;
};

/**
 * Check to see if another change has started since updateId started.
 * This allows us to bail out of an update.
 * It's hard to make updates atomic because until you've responded to a parse
 * of the command argument, you don't know how to parse the arguments to that
 * command.
 */
Requisition.prototype._isChangeCurrent = function(updateId) {
  return updateId + 1 === this._nextUpdateId;
};

/**
 * See notes on beginChange
 */
Requisition.prototype._endChangeCheckOrder = function(updateId) {
  if (updateId + 1 !== this._nextUpdateId) {
    // An update that started after we did has already finished, so our
    // changes are out of date. Abandon further work.
    return false;
  }

  return true;
};

var legacy = false;

/**
 * Functions and data related to the execution of a command
 */
Object.defineProperty(Requisition.prototype, 'executionContext', {
  get: function() {
    if (this._executionContext == null) {
      this._executionContext = {
        defer: defer,
        typedData: function(type, data) {
          return {
            isTypedData: true,
            data: data,
            type: type
          };
        },
        getArgsObject: this.getArgsObject.bind(this)
      };

      // Alias requisition so we're clear about what's what
      var requisition = this;
      Object.defineProperty(this._executionContext, 'prefix', {
        get: function() { return requisition.prefix; },
        enumerable: true
      });
      Object.defineProperty(this._executionContext, 'typed', {
        get: function() { return requisition.toString(); },
        enumerable: true
      });
      Object.defineProperty(this._executionContext, 'environment', {
        get: function() { return requisition.environment; },
        enumerable: true
      });
      Object.defineProperty(this._executionContext, 'shell', {
        get: function() { return requisition.shell; },
        enumerable: true
      });
      Object.defineProperty(this._executionContext, 'system', {
        get: function() { return requisition.system; },
        enumerable: true
      });

      this._executionContext.updateExec = this._contextUpdateExec.bind(this);

      if (legacy) {
        this._executionContext.createView = view.createView;
        this._executionContext.exec = this.exec.bind(this);
        this._executionContext.update = this._contextUpdate.bind(this);

        Object.defineProperty(this._executionContext, 'document', {
          get: function() { return requisition.document; },
          enumerable: true
        });
      }
    }

    return this._executionContext;
  },
  enumerable: true
});

/**
 * Functions and data related to the conversion of the output of a command
 */
Object.defineProperty(Requisition.prototype, 'conversionContext', {
  get: function() {
    if (this._conversionContext == null) {
      this._conversionContext = {
        defer: defer,

        createView: view.createView,
        exec: this.exec.bind(this),
        update: this._contextUpdate.bind(this),
        updateExec: this._contextUpdateExec.bind(this)
      };

      // Alias requisition so we're clear about what's what
      var requisition = this;

      Object.defineProperty(this._conversionContext, 'document', {
        get: function() { return requisition.document; },
        enumerable: true
      });
      Object.defineProperty(this._conversionContext, 'environment', {
        get: function() { return requisition.environment; },
        enumerable: true
      });
      Object.defineProperty(this._conversionContext, 'system', {
        get: function() { return requisition.system; },
        enumerable: true
      });
    }

    return this._conversionContext;
  },
  enumerable: true
});

/**
 * Assignments have an order, so we need to store them in an array.
 * But we also need named access ...
 * @return The found assignment, or undefined, if no match was found
 */
Requisition.prototype.getAssignment = function(nameOrNumber) {
  var name = (typeof nameOrNumber === 'string') ?
    nameOrNumber :
    Object.keys(this._assignments)[nameOrNumber];
  return this._assignments[name] || undefined;
};

/**
 * Where parameter name == assignment names - they are the same
 */
Requisition.prototype.getParameterNames = function() {
  return Object.keys(this._assignments);
};

/**
 * The overall status is the most severe status.
 * There is no such thing as an INCOMPLETE overall status because the
 * definition of INCOMPLETE takes into account the cursor position to say 'this
 * isn't quite ERROR because the user can fix it by typing', however overall,
 * this is still an error status.
 */
Object.defineProperty(Requisition.prototype, 'status', {
  get: function() {
    var status = Status.VALID;
    if (this._unassigned.length !== 0) {
      var isAllIncomplete = true;
      this._unassigned.forEach(function(assignment) {
        if (!assignment.param.type.isIncompleteName) {
          isAllIncomplete = false;
        }
      });
      status = isAllIncomplete ? Status.INCOMPLETE : Status.ERROR;
    }

    this.getAssignments(true).forEach(function(assignment) {
      var assignStatus = assignment.getStatus();
      if (assignStatus > status) {
        status = assignStatus;
      }
    }, this);
    if (status === Status.INCOMPLETE) {
      status = Status.ERROR;
    }
    return status;
  },
  enumerable : true
});

/**
 * If ``requisition.status != VALID`` message then return a string which
 * best describes what is wrong. Generally error messages are delivered by
 * looking at the error associated with the argument at the cursor, but there
 * are times when you just want to say 'tell me the worst'.
 * If ``requisition.status != VALID`` then return ``null``.
 */
Requisition.prototype.getStatusMessage = function() {
  if (this.commandAssignment.getStatus() !== Status.VALID) {
    return l10n.lookupFormat('cliUnknownCommand2',
                             [ this.commandAssignment.arg.text ]);
  }

  var assignments = this.getAssignments();
  for (var i = 0; i < assignments.length; i++) {
    if (assignments[i].getStatus() !== Status.VALID) {
      return assignments[i].message;
    }
  }

  if (this._unassigned.length !== 0) {
    return l10n.lookup('cliUnusedArg');
  }

  return null;
};

/**
 * Extract the names and values of all the assignments, and return as
 * an object.
 */
Requisition.prototype.getArgsObject = function() {
  var args = {};
  this.getAssignments().forEach(function(assignment) {
    args[assignment.param.name] = assignment.conversion.isDataProvided() ?
            assignment.value :
            assignment.param.defaultValue;
  }, this);
  return args;
};

/**
 * Access the arguments as an array.
 * @param includeCommand By default only the parameter arguments are
 * returned unless (includeCommand === true), in which case the list is
 * prepended with commandAssignment.arg
 */
Requisition.prototype.getAssignments = function(includeCommand) {
  var assignments = [];
  if (includeCommand === true) {
    assignments.push(this.commandAssignment);
  }
  Object.keys(this._assignments).forEach(function(name) {
    assignments.push(this.getAssignment(name));
  }, this);
  return assignments;
};

/**
 * There are a few places where we need to know what the 'next thing' is. What
 * is the user going to be filling out next (assuming they don't enter a named
 * argument). The next argument is the first in line that is both blank, and
 * that can be filled in positionally.
 * @return The next assignment to be used, or null if all the positional
 * parameters have values.
 */
Requisition.prototype._getFirstBlankPositionalAssignment = function() {
  var reply = null;
  Object.keys(this._assignments).some(function(name) {
    var assignment = this.getAssignment(name);
    if (assignment.arg.type === 'BlankArgument' &&
            assignment.param.isPositionalAllowed) {
      reply = assignment;
      return true; // i.e. break
    }
    return false;
  }, this);
  return reply;
};

/**
 * The update process is asynchronous, so there is (unavoidably) a window
 * where we've worked out the command but don't yet understand all the params.
 * If we try to do things to a requisition in this window we may get
 * inconsistent results. Asynchronous promises have made the window bigger.
 * The only time we've seen this in practice is during focus events due to
 * clicking on a shortcut. The focus want to check the cursor position while
 * the shortcut is updating the command line.
 * This function allows us to detect and back out of this problem.
 * We should be able to remove this function when all the state in a
 * requisition can be encapsulated and updated atomically.
 */
Requisition.prototype.isUpToDate = function() {
  if (!this._args) {
    return false;
  }
  for (var i = 0; i < this._args.length; i++) {
    if (this._args[i].assignment == null) {
      return false;
    }
  }
  return true;
};

/**
 * Look through the arguments attached to our assignments for the assignment
 * at the given position.
 * @param {number} cursor The cursor position to query
 */
Requisition.prototype.getAssignmentAt = function(cursor) {
  // We short circuit this one because we may have no args, or no args with
  // any size and the alg below only finds arguments with size.
  if (cursor === 0) {
    return this.commandAssignment;
  }

  var assignForPos = [];
  var i, j;
  for (i = 0; i < this._args.length; i++) {
    var arg = this._args[i];
    var assignment = arg.assignment;

    // prefix and text are clearly part of the argument
    for (j = 0; j < arg.prefix.length; j++) {
      assignForPos.push(assignment);
    }
    for (j = 0; j < arg.text.length; j++) {
      assignForPos.push(assignment);
    }

    // suffix is part of the argument only if this is a named parameter,
    // otherwise it looks forwards
    if (arg.assignment.arg.type === 'NamedArgument') {
      // leave the argument as it is
    }
    else if (this._args.length > i + 1) {
      // first to the next argument
      assignment = this._args[i + 1].assignment;
    }
    else {
      // then to the first blank positional parameter, leaving 'as is' if none
      var nextAssignment = this._getFirstBlankPositionalAssignment();
      if (nextAssignment != null) {
        assignment = nextAssignment;
      }
    }

    for (j = 0; j < arg.suffix.length; j++) {
      assignForPos.push(assignment);
    }
  }

  // Possible shortcut, we don't really need to go through all the args
  // to work out the solution to this

  return assignForPos[cursor - 1];
};

/**
 * Extract a canonical version of the input
 * @return a promise of a string which is the canonical version of what was
 * typed
 */
Requisition.prototype.toCanonicalString = function() {
  var cmd = this.commandAssignment.value ?
      this.commandAssignment.value.name :
      this.commandAssignment.arg.text;

  // Canonically, if we've opened with a { then we should have a } to close
  var lineSuffix = '';
  if (cmd === '{') {
    var scriptSuffix = this.getAssignment(0).arg.suffix;
    lineSuffix = (scriptSuffix.indexOf('}') === -1) ? ' }' : '';
  }

  var ctx = this.executionContext;

  // First stringify all the arguments
  var argPromise = util.promiseEach(this.getAssignments(), function(assignment) {
    // Bug 664377: This will cause problems if there is a non-default value
    // after a default value. Also we need to decide when to use
    // named parameters in place of positional params. Both can wait.
    if (assignment.value === assignment.param.defaultValue) {
      return '';
    }

    var val = assignment.param.type.stringify(assignment.value, ctx);
    return Promise.resolve(val).then(function(str) {
      return ' ' + str;
    }.bind(this));
  }.bind(this));

  return argPromise.then(function(strings) {
    return cmd + strings.join('') + lineSuffix;
  }.bind(this));
};

/**
 * Reconstitute the input from the args
 */
Requisition.prototype.toString = function() {
  if (!this._args) {
    throw new Error('toString requires a command line. See source.');
  }

  return this._args.map(function(arg) {
    return arg.toString();
  }).join('');
};

/**
 * For test/debug use only. The output from this function is subject to wanton
 * random change without notice, and should not be relied upon to even exist
 * at some later date.
 */
Object.defineProperty(Requisition.prototype, '_summaryJson', {
  get: function() {
    var summary = {
      $args: this._args.map(function(arg) {
        return arg._summaryJson;
      }),
      _command: this.commandAssignment._summaryJson,
      _unassigned: this._unassigned.forEach(function(assignment) {
        return assignment._summaryJson;
      })
    };

    Object.keys(this._assignments).forEach(function(name) {
      summary[name] = this.getAssignment(name)._summaryJson;
    }.bind(this));

    return summary;
  },
  enumerable: true
});

/**
 * When any assignment changes, we might need to update the _args array to
 * match and inform people of changes to the typed input text.
 */
Requisition.prototype._setAssignmentInternal = function(assignment, conversion) {
  var oldConversion = assignment.conversion;

  assignment.conversion = conversion;
  assignment.conversion.assignment = assignment;

  // Do nothing if the conversion is unchanged
  if (assignment.conversion.equals(oldConversion)) {
    if (assignment === this.commandAssignment) {
      this._setBlankArguments();
    }
    return;
  }

  // When the command changes, we need to keep a bunch of stuff in sync
  if (assignment === this.commandAssignment) {
    this._assignments = {};

    var command = this.commandAssignment.value;
    if (command) {
      for (var i = 0; i < command.params.length; i++) {
        var param = command.params[i];
        var newAssignment = new Assignment(param);
        this._setBlankAssignment(newAssignment);
        this._assignments[param.name] = newAssignment;
      }
    }
    this.assignmentCount = Object.keys(this._assignments).length;
  }
};

/**
 * Internal function to alter the given assignment using the given arg.
 * @param assignment The assignment to alter
 * @param arg The new value for the assignment. An instance of Argument, or an
 * instance of Conversion, or null to set the blank value.
 * @param options There are a number of ways to customize how the assignment
 * is made, including:
 * - internal: (default:false) External updates are required to do more work,
 *   including adjusting the args in this requisition to stay in sync.
 *   On the other hand non internal changes use beginChange to back out of
 *   changes when overtaken asynchronously.
 *   Setting internal:true effectively means this is being called as part of
 *   the update process.
 * - matchPadding: (default:false) Alter the whitespace on the prefix and
 *   suffix of the new argument to match that of the old argument. This only
 *   makes sense with internal=false
 * @return A promise that resolves to undefined when the assignment is complete
 */
Requisition.prototype.setAssignment = function(assignment, arg, options) {
  options = options || {};
  if (!options.internal) {
    var originalArgs = assignment.arg.getArgs();

    // Update the args array
    var replacementArgs = arg.getArgs();
    var maxLen = Math.max(originalArgs.length, replacementArgs.length);
    for (var i = 0; i < maxLen; i++) {
      // If there are no more original args, or if the original arg was blank
      // (i.e. not typed by the user), we'll just need to add at the end
      if (i >= originalArgs.length || originalArgs[i].type === 'BlankArgument') {
        this._args.push(replacementArgs[i]);
        continue;
      }

      var index = this._args.indexOf(originalArgs[i]);
      if (index === -1) {
        console.error('Couldn\'t find ', originalArgs[i], ' in ', this._args);
        throw new Error('Couldn\'t find ' + originalArgs[i]);
      }

      // If there are no more replacement args, we just remove the original args
      // Otherwise swap original args and replacements
      if (i >= replacementArgs.length) {
        this._args.splice(index, 1);
      }
      else {
        if (options.matchPadding) {
          if (replacementArgs[i].prefix.length === 0 &&
              this._args[index].prefix.length !== 0) {
            replacementArgs[i].prefix = this._args[index].prefix;
          }
          if (replacementArgs[i].suffix.length === 0 &&
              this._args[index].suffix.length !== 0) {
            replacementArgs[i].suffix = this._args[index].suffix;
          }
        }
        this._args[index] = replacementArgs[i];
      }
    }
  }

  var updateId = options.internal ? null : this._beginChange();

  var setAssignmentInternal = function(conversion) {
    if (options.internal || this._isChangeCurrent(updateId)) {
      this._setAssignmentInternal(assignment, conversion);
    }

    if (!options.internal) {
      this._endChangeCheckOrder(updateId);
    }

    return Promise.resolve(undefined);
  }.bind(this);

  if (arg == null) {
    var blank = assignment.param.type.getBlank(this.executionContext);
    return setAssignmentInternal(blank);
  }

  if (typeof arg.getStatus === 'function') {
    // It's not really an arg, it's a conversion already
    return setAssignmentInternal(arg);
  }

  var parsed = assignment.param.type.parse(arg, this.executionContext);
  return parsed.then(setAssignmentInternal);
};

/**
 * Reset an assignment to its default value.
 * For internal use only.
 * Happens synchronously.
 */
Requisition.prototype._setBlankAssignment = function(assignment) {
  var blank = assignment.param.type.getBlank(this.executionContext);
  this._setAssignmentInternal(assignment, blank);
};

/**
 * Reset all the assignments to their default values.
 * For internal use only.
 * Happens synchronously.
 */
Requisition.prototype._setBlankArguments = function() {
  this.getAssignments().forEach(this._setBlankAssignment.bind(this));
};

/**
 * Input trace gives us an array of Argument tracing objects, one for each
 * character in the typed input, from which we can derive information about how
 * to display this typed input. It's a bit like toString on steroids.
 * <p>
 * The returned object has the following members:<ul>
 * <li>character: The character to which this arg trace refers.
 * <li>arg: The Argument to which this character is assigned.
 * <li>part: One of ['prefix'|'text'|suffix'] - how was this char understood
 * </ul>
 * <p>
 * The Argument objects are as output from tokenize() rather than as applied
 * to Assignments by _assign() (i.e. they are not instances of NamedArgument,
 * ArrayArgument, etc).
 * <p>
 * To get at the arguments applied to the assignments simply call
 * <tt>arg.assignment.arg</tt>. If <tt>arg.assignment.arg !== arg</tt> then
 * the arg applied to the assignment will contain the original arg.
 * See _assign() for details.
 */
Requisition.prototype.createInputArgTrace = function() {
  if (!this._args) {
    throw new Error('createInputMap requires a command line. See source.');
  }

  var args = [];
  var i;
  this._args.forEach(function(arg) {
    for (i = 0; i < arg.prefix.length; i++) {
      args.push({ arg: arg, character: arg.prefix[i], part: 'prefix' });
    }
    for (i = 0; i < arg.text.length; i++) {
      args.push({ arg: arg, character: arg.text[i], part: 'text' });
    }
    for (i = 0; i < arg.suffix.length; i++) {
      args.push({ arg: arg, character: arg.suffix[i], part: 'suffix' });
    }
  });

  return args;
};

/**
 * If the last character is whitespace then things that we suggest to add to
 * the end don't need a space prefix.
 * While this is quite a niche function, it has 2 benefits:
 * - it's more correct because we can distinguish between final whitespace that
 *   is part of an unclosed string, and parameter separating whitespace.
 * - also it's faster than toString() the whole thing and checking the end char
 * @return true iff the last character is interpreted as parameter separating
 * whitespace
 */
Requisition.prototype.typedEndsWithSeparator = function() {
  if (!this._args) {
    throw new Error('typedEndsWithSeparator requires a command line. See source.');
  }

  if (this._args.length === 0) {
    return false;
  }

  // This is not as easy as doing (this.toString().slice(-1) === ' ')
  // See the doc comments above; We're checking for separators, not spaces
  var lastArg = this._args.slice(-1)[0];
  if (lastArg.suffix.slice(-1) === ' ') {
    return true;
  }

  return lastArg.text === '' && lastArg.suffix === ''
      && lastArg.prefix.slice(-1) === ' ';
};

/**
 * Return an array of Status scores so we can create a marked up
 * version of the command line input.
 * @param cursor We only take a status of INCOMPLETE to be INCOMPLETE when the
 * cursor is actually in the argument. Otherwise it's an error.
 * @return Array of objects each containing <tt>status</tt> property and a
 * <tt>string</tt> property containing the characters to which the status
 * applies. Concatenating the strings in order gives the original input.
 */
Requisition.prototype.getInputStatusMarkup = function(cursor) {
  var argTraces = this.createInputArgTrace();
  // Generally the 'argument at the cursor' is the argument before the cursor
  // unless it is before the first char, in which case we take the first.
  cursor = cursor === 0 ? 0 : cursor - 1;
  var cTrace = argTraces[cursor];

  var markup = [];
  for (var i = 0; i < argTraces.length; i++) {
    var argTrace = argTraces[i];
    var arg = argTrace.arg;
    var status = Status.VALID;
    if (argTrace.part === 'text') {
      status = arg.assignment.getStatus(arg);
      // Promote INCOMPLETE to ERROR  ...
      if (status === Status.INCOMPLETE) {
        // If the cursor is in the prefix or suffix of an argument then we
        // don't consider it in the argument for the purposes of preventing
        // the escalation to ERROR. However if this is a NamedArgument, then we
        // allow the suffix (as space between 2 parts of the argument) to be in.
        // We use arg.assignment.arg not arg because we're looking at the arg
        // that got put into the assignment not as returned by tokenize()
        var isNamed = (cTrace.arg.assignment.arg.type === 'NamedArgument');
        var isInside = cTrace.part === 'text' ||
                        (isNamed && cTrace.part === 'suffix');
        if (arg.assignment !== cTrace.arg.assignment || !isInside) {
          // And if we're not in the command
          if (!(arg.assignment instanceof CommandAssignment)) {
            status = Status.ERROR;
          }
        }
      }
    }

    markup.push({ status: status, string: argTrace.character });
  }

  // De-dupe: merge entries where 2 adjacent have same status
  i = 0;
  while (i < markup.length - 1) {
    if (markup[i].status === markup[i + 1].status) {
      markup[i].string += markup[i + 1].string;
      markup.splice(i + 1, 1);
    }
    else {
      i++;
    }
  }

  return markup;
};

/**
 * Describe the state of the current input in a way that allows display of
 * predictions and completion hints
 * @param start The location of the cursor
 * @param rank The index of the chosen prediction
 * @return A promise of an object containing the following properties:
 * - statusMarkup: An array of Status scores so we can create a marked up
 *   version of the command line input. See getInputStatusMarkup() for details
 * - unclosedJs: Is the entered command a JS command with no closing '}'?
 * - directTabText: A promise of the text that we *add* to the command line
 *   when TAB is pressed, to be displayed directly after the cursor. See also
 *   arrowTabText.
 * - emptyParameters: A promise of the text that describes the arguments that
 *   the user is yet to type.
 * - arrowTabText: A promise of the text that *replaces* the current argument
 *   when TAB is pressed, generally displayed after a "|->" symbol. See also
 *   directTabText.
 */
Requisition.prototype.getStateData = function(start, rank) {
  var typed = this.toString();
  var current = this.getAssignmentAt(start);
  var context = this.executionContext;
  var predictionPromise = (typed.trim().length !== 0) ?
                          current.getPredictionRanked(context, rank) :
                          Promise.resolve(null);

  return predictionPromise.then(function(prediction) {
    // directTabText is for when the current input is a prefix of the completion
    // arrowTabText is for when we need to use an -> to show what will be used
    var directTabText = '';
    var arrowTabText = '';
    var emptyParameters = [];

    if (typed.trim().length !== 0) {
      var cArg = current.arg;

      if (prediction) {
        var tabText = prediction.name;
        var existing = cArg.text;

        // Normally the cursor being just before whitespace means that you are
        // 'in' the previous argument, which means that the prediction is based
        // on that argument, however NamedArguments break this by having 2 parts
        // so we need to prepend the tabText with a space for NamedArguments,
        // but only when there isn't already a space at the end of the prefix
        // (i.e. ' --name' not ' --name ')
        if (current.isInName()) {
          tabText = ' ' + tabText;
        }

        if (existing !== tabText) {
          // Decide to use directTabText or arrowTabText
          // Strip any leading whitespace from the user inputted value because
          // the tabText will never have leading whitespace.
          var inputValue = existing.replace(/^\s*/, '');
          var isStrictCompletion = tabText.indexOf(inputValue) === 0;
          if (isStrictCompletion && start === typed.length) {
            // Display the suffix of the prediction as the completion
            var numLeadingSpaces = existing.match(/^(\s*)/)[0].length;

            directTabText = tabText.slice(existing.length - numLeadingSpaces);
          }
          else {
            // Display the '-> prediction' at the end of the completer element
            // \u21E5 is the JS escape right arrow
            arrowTabText = '\u21E5 ' + tabText;
          }
        }
      }
      else {
        // There's no prediction, but if this is a named argument that needs a
        // value (that is without any) then we need to show that one is needed
        // For example 'git commit --message ', clearly needs some more text
        if (cArg.type === 'NamedArgument' && cArg.valueArg == null) {
          emptyParameters.push('<' + current.param.type.name + '>\u00a0');
        }
      }
    }

    // Add a space between the typed text (+ directTabText) and the hints,
    // making sure we don't add 2 sets of padding
    if (directTabText !== '') {
      directTabText += '\u00a0'; // a.k.a &nbsp;
    }
    else if (!this.typedEndsWithSeparator()) {
      emptyParameters.unshift('\u00a0');
    }

    // Calculate the list of parameters to be filled in
    // We generate an array of emptyParameter markers for each positional
    // parameter to the current command.
    // Generally each emptyParameter marker begins with a space to separate it
    // from whatever came before, unless what comes before ends in a space.

    this.getAssignments().forEach(function(assignment) {
      // Named arguments are handled with a group [options] marker
      if (!assignment.param.isPositionalAllowed) {
        return;
      }

      // No hints if we've got content for this parameter
      if (assignment.arg.toString().trim() !== '') {
        return;
      }

      // No hints if we have a prediction
      if (directTabText !== '' && current === assignment) {
        return;
      }

      var text = (assignment.param.isDataRequired) ?
          '<' + assignment.param.name + '>\u00a0' :
          '[' + assignment.param.name + ']\u00a0';

      emptyParameters.push(text);
    }.bind(this));

    var command = this.commandAssignment.value;
    var addOptionsMarker = false;

    // We add an '[options]' marker when there are named parameters that are
    // not filled in and not hidden, and we don't have any directTabText
    if (command && command.hasNamedParameters) {
      command.params.forEach(function(param) {
        var arg = this.getAssignment(param.name).arg;
        if (!param.isPositionalAllowed && !param.hidden
                && arg.type === 'BlankArgument') {
          addOptionsMarker = true;
        }
      }, this);
    }

    if (addOptionsMarker) {
      // Add an nbsp if we don't have one at the end of the input or if
      // this isn't the first param we've mentioned
      emptyParameters.push('[options]\u00a0');
    }

    // Is the entered command a JS command with no closing '}'?
    var unclosedJs = command && command.name === '{' &&
                     this.getAssignment(0).arg.suffix.indexOf('}') === -1;

    return {
      statusMarkup: this.getInputStatusMarkup(start),
      unclosedJs: unclosedJs,
      directTabText: directTabText,
      arrowTabText: arrowTabText,
      emptyParameters: emptyParameters
    };
  }.bind(this));
};

/**
 * Pressing TAB sometimes requires that we add a space to denote that we're on
 * to the 'next thing'.
 * @param assignment The assignment to which to append the space
 */
Requisition.prototype._addSpace = function(assignment) {
  var arg = assignment.arg.beget({ suffixSpace: true });
  if (arg !== assignment.arg) {
    return this.setAssignment(assignment, arg);
  }
  else {
    return Promise.resolve(undefined);
  }
};

/**
 * Complete the argument at <tt>cursor</tt>.
 * Basically the same as:
 *   assignment = getAssignmentAt(cursor);
 *   assignment.value = assignment.conversion.predictions[0];
 * Except it's done safely, and with particular care to where we place the
 * space, which is complex, and annoying if we get it wrong.
 *
 * WARNING: complete() can happen asynchronously.
 *
 * @param cursor The cursor configuration. Should have start and end properties
 * which should be set to start and end of the selection.
 * @param rank The index of the prediction that we should choose.
 * This number is not bounded by the size of the prediction array, we take the
 * modulus to get it within bounds
 * @return A promise which completes (with undefined) when any outstanding
 * completion tasks are done.
 */
Requisition.prototype.complete = function(cursor, rank) {
  var assignment = this.getAssignmentAt(cursor.start);

  var context = this.executionContext;
  var predictionPromise = assignment.getPredictionRanked(context, rank);
  return predictionPromise.then(function(prediction) {
    var outstanding = [];

    // Note: Since complete is asynchronous we should perhaps have a system to
    // bail out of making changes if the command line has changed since TAB
    // was pressed. It's not yet clear if this will be a problem.

    if (prediction == null) {
      // No predictions generally means we shouldn't change anything on TAB,
      // but TAB has the connotation of 'next thing' and when we're at the end
      // of a thing that implies that we should add a space. i.e.
      // 'help<TAB>' -> 'help '
      // But we should only do this if the thing that we're 'completing' is
      // valid and doesn't already end in a space.
      if (assignment.arg.suffix.slice(-1) !== ' ' &&
              assignment.getStatus() === Status.VALID) {
        outstanding.push(this._addSpace(assignment));
      }

      // Also add a space if we are in the name part of an assignment, however
      // this time we don't want the 'push the space to the next assignment'
      // logic, so we don't use addSpace
      if (assignment.isInName()) {
        var newArg = assignment.arg.beget({ prefixPostSpace: true });
        outstanding.push(this.setAssignment(assignment, newArg));
      }
    }
    else {
      // Mutate this argument to hold the completion
      var arg = assignment.arg.beget({
        text: prediction.name,
        dontQuote: (assignment === this.commandAssignment)
      });
      var assignPromise = this.setAssignment(assignment, arg);

      if (!prediction.incomplete) {
        assignPromise = assignPromise.then(function() {
          // The prediction is complete, add a space to let the user move-on
          return this._addSpace(assignment).then(function() {
            // Bug 779443 - Remove or explain the re-parse
            if (assignment instanceof UnassignedAssignment) {
              return this.update(this.toString());
            }
          }.bind(this));
        }.bind(this));
      }

      outstanding.push(assignPromise);
    }

    return Promise.all(outstanding).then(function() {
      return true;
    }.bind(this));
  }.bind(this));
};

/**
 * Replace the current value with the lower value if such a concept exists.
 */
Requisition.prototype.nudge = function(assignment, by) {
  var ctx = this.executionContext;
  var val = assignment.param.type.nudge(assignment.value, by, ctx);
  return Promise.resolve(val).then(function(replacement) {
    if (replacement != null) {
      var val = assignment.param.type.stringify(replacement, ctx);
      return Promise.resolve(val).then(function(str) {
        var arg = assignment.arg.beget({ text: str });
        return this.setAssignment(assignment, arg);
      }.bind(this));
    }
  }.bind(this));
};

/**
 * Helper to find the 'data-command' attribute, used by |update()|
 */
function getDataCommandAttribute(element) {
  var command = element.getAttribute('data-command');
  if (!command) {
    command = element.querySelector('*[data-command]')
                     .getAttribute('data-command');
  }
  return command;
}

/**
 * Designed to be called from context.update(). Acts just like update() except
 * that it also calls onExternalUpdate() to inform the UI of an unexpected
 * change to the current command.
 */
Requisition.prototype._contextUpdate = function(typed) {
  return this.update(typed).then(function(reply) {
    this.onExternalUpdate({ typed: typed });
    return reply;
  }.bind(this));
};

/**
 * Called by the UI when ever the user interacts with a command line input
 * @param typed The contents of the input field OR an HTML element (or an event
 * that targets an HTML element) which has a data-command attribute or a child
 * with the same that contains the command to update with
 */
Requisition.prototype.update = function(typed) {
  // Should be "if (typed instanceof HTMLElement)" except Gecko
  if (typeof typed.querySelector === 'function') {
    typed = getDataCommandAttribute(typed);
  }
  // Should be "if (typed instanceof Event)" except Gecko
  if (typeof typed.currentTarget === 'object') {
    typed = getDataCommandAttribute(typed.currentTarget);
  }

  var updateId = this._beginChange();

  this._args = exports.tokenize(typed);
  var args = this._args.slice(0); // i.e. clone

  this._split(args);

  return this._assign(args).then(function() {
    return this._endChangeCheckOrder(updateId);
  }.bind(this));
};

/**
 * Similar to update('') except that it's guaranteed to execute synchronously
 */
Requisition.prototype.clear = function() {
  var arg = new Argument('', '', '');
  this._args = [ arg ];

  var conversion = commandModule.parse(this.executionContext, arg, false);
  this.setAssignment(this.commandAssignment, conversion, { internal: true });
};

/**
 * tokenize() is a state machine. These are the states.
 */
var In = {
  /**
   * The last character was ' '.
   * Typing a ' ' character will not change the mode
   * Typing one of '"{ will change mode to SINGLE_Q, DOUBLE_Q or SCRIPT.
   * Anything else goes into SIMPLE mode.
   */
  WHITESPACE: 1,

  /**
   * The last character was part of a parameter.
   * Typing ' ' returns to WHITESPACE mode. Any other character
   * (including '"{} which are otherwise special) does not change the mode.
   */
  SIMPLE: 2,

  /**
   * We're inside single quotes: '
   * Typing ' returns to WHITESPACE mode. Other characters do not change mode.
   */
  SINGLE_Q: 3,

  /**
   * We're inside double quotes: "
   * Typing " returns to WHITESPACE mode. Other characters do not change mode.
   */
  DOUBLE_Q: 4,

  /**
   * We're inside { and }
   * Typing } returns to WHITESPACE mode. Other characters do not change mode.
   * SCRIPT mode is slightly different from other modes in that spaces between
   * the {/} delimiters and the actual input are not considered significant.
   * e.g: " x " is a 3 character string, delimited by double quotes, however
   * { x } is a 1 character JavaScript surrounded by whitespace and {}
   * delimiters.
   * In the short term we assume that the JS routines can make sense of the
   * extra whitespace, however at some stage we may need to move the space into
   * the Argument prefix/suffix.
   * Also we don't attempt to handle nested {}. See bug 678961
   */
  SCRIPT: 5
};

/**
 * Split up the input taking into account ', " and {.
 * We don't consider \t or other classical whitespace characters to split
 * arguments apart. For one thing these characters are hard to type, but also
 * if the user has gone to the trouble of pasting a TAB character into the
 * input field (or whatever it takes), they probably mean it.
 */
exports.tokenize = function(typed) {
  // For blank input, place a dummy empty argument into the list
  if (typed == null || typed.length === 0) {
    return [ new Argument('', '', '') ];
  }

  if (isSimple(typed)) {
    return [ new Argument(typed, '', '') ];
  }

  var mode = In.WHITESPACE;

  // First we swap out escaped characters that are special to the tokenizer.
  // So a backslash followed by any of ['"{} ] is turned into a unicode private
  // char so we can swap back later
  typed = typed
      .replace(/\\\\/g, '\uF000')
      .replace(/\\ /g, '\uF001')
      .replace(/\\'/g, '\uF002')
      .replace(/\\"/g, '\uF003')
      .replace(/\\{/g, '\uF004')
      .replace(/\\}/g, '\uF005');

  function unescape2(escaped) {
    return escaped
        .replace(/\uF000/g, '\\\\')
        .replace(/\uF001/g, '\\ ')
        .replace(/\uF002/g, '\\\'')
        .replace(/\uF003/g, '\\\"')
        .replace(/\uF004/g, '\\{')
        .replace(/\uF005/g, '\\}');
  }

  var i = 0;          // The index of the current character
  var start = 0;      // Where did this section start?
  var prefix = '';    // Stuff that comes before the current argument
  var args = [];      // The array that we're creating
  var blockDepth = 0; // For JS with nested {}

  // This is just a state machine. We're going through the string char by char
  // The 'mode' is one of the 'In' states. As we go, we're adding Arguments
  // to the 'args' array.

  while (true) {
    var c = typed[i];
    var str;
    switch (mode) {
      case In.WHITESPACE:
        if (c === '\'') {
          prefix = typed.substring(start, i + 1);
          mode = In.SINGLE_Q;
          start = i + 1;
        }
        else if (c === '"') {
          prefix = typed.substring(start, i + 1);
          mode = In.DOUBLE_Q;
          start = i + 1;
        }
        else if (c === '{') {
          prefix = typed.substring(start, i + 1);
          mode = In.SCRIPT;
          blockDepth++;
          start = i + 1;
        }
        else if (/ /.test(c)) {
          // Still whitespace, do nothing
        }
        else {
          prefix = typed.substring(start, i);
          mode = In.SIMPLE;
          start = i;
        }
        break;

      case In.SIMPLE:
        // There is an edge case of xx'xx which we are assuming to
        // be a single parameter (and same with ")
        if (c === ' ') {
          str = unescape2(typed.substring(start, i));
          args.push(new Argument(str, prefix, ''));
          mode = In.WHITESPACE;
          start = i;
          prefix = '';
        }
        break;

      case In.SINGLE_Q:
        if (c === '\'') {
          str = unescape2(typed.substring(start, i));
          args.push(new Argument(str, prefix, c));
          mode = In.WHITESPACE;
          start = i + 1;
          prefix = '';
        }
        break;

      case In.DOUBLE_Q:
        if (c === '"') {
          str = unescape2(typed.substring(start, i));
          args.push(new Argument(str, prefix, c));
          mode = In.WHITESPACE;
          start = i + 1;
          prefix = '';
        }
        break;

      case In.SCRIPT:
        if (c === '{') {
          blockDepth++;
        }
        else if (c === '}') {
          blockDepth--;
          if (blockDepth === 0) {
            str = unescape2(typed.substring(start, i));
            args.push(new ScriptArgument(str, prefix, c));
            mode = In.WHITESPACE;
            start = i + 1;
            prefix = '';
          }
        }
        break;
    }

    i++;

    if (i >= typed.length) {
      // There is nothing else to read - tidy up
      if (mode === In.WHITESPACE) {
        if (i !== start) {
          // There's whitespace at the end of the typed string. Add it to the
          // last argument's suffix, creating an empty argument if needed.
          var extra = typed.substring(start, i);
          var lastArg = args[args.length - 1];
          if (!lastArg) {
            args.push(new Argument('', extra, ''));
          }
          else {
            lastArg.suffix += extra;
          }
        }
      }
      else if (mode === In.SCRIPT) {
        str = unescape2(typed.substring(start, i + 1));
        args.push(new ScriptArgument(str, prefix, ''));
      }
      else {
        str = unescape2(typed.substring(start, i + 1));
        args.push(new Argument(str, prefix, ''));
      }
      break;
    }
  }

  return args;
};

/**
 * If the input has no spaces, quotes, braces or escapes,
 * we can take the fast track.
 */
function isSimple(typed) {
   for (var i = 0; i < typed.length; i++) {
     var c = typed.charAt(i);
     if (c === ' ' || c === '"' || c === '\'' ||
         c === '{' || c === '}' || c === '\\') {
       return false;
     }
   }
   return true;
}

/**
 * Looks in the commands for a command extension that matches what has been
 * typed at the command line.
 */
Requisition.prototype._split = function(args) {
  // Handle the special case of the user typing { javascript(); }
  // We use the hidden 'eval' command directly rather than shift()ing one of
  // the parameters, and parse()ing it.
  var conversion;
  if (args[0].type === 'ScriptArgument') {
    // Special case: if the user enters { console.log('foo'); } then we need to
    // use the hidden 'eval' command
    var command = this.system.commands.get(evalCmd.name);
    conversion = new Conversion(command, new ScriptArgument());
    this._setAssignmentInternal(this.commandAssignment, conversion);
    return;
  }

  var argsUsed = 1;

  while (argsUsed <= args.length) {
    var arg = (argsUsed === 1) ?
              args[0] :
              new MergedArgument(args, 0, argsUsed);

    if (this.prefix != null && this.prefix !== '') {
      var prefixArg = new Argument(this.prefix, '', ' ');
      var prefixedArg = new MergedArgument([ prefixArg, arg ]);

      conversion = commandModule.parse(this.executionContext, prefixedArg, false);
      if (conversion.value == null) {
        conversion = commandModule.parse(this.executionContext, arg, false);
      }
    }
    else {
      conversion = commandModule.parse(this.executionContext, arg, false);
    }

    // We only want to carry on if this command is a parent command,
    // which means that there is a commandAssignment, but not one with
    // an exec function.
    if (!conversion.value || conversion.value.exec) {
      break;
    }

    // Previously we needed a way to hide commands depending context.
    // We have not resurrected that feature yet, but if we do we should
    // insert code here to ignore certain commands depending on the
    // context/environment

    argsUsed++;
  }

  // This could probably be re-written to consume args as we go
  for (var i = 0; i < argsUsed; i++) {
    args.shift();
  }

  this._setAssignmentInternal(this.commandAssignment, conversion);
};

/**
 * Add all the passed args to the list of unassigned assignments.
 */
Requisition.prototype._addUnassignedArgs = function(args) {
  args.forEach(function(arg) {
    this._unassigned.push(new UnassignedAssignment(this, arg));
  }.bind(this));

  return RESOLVED;
};

/**
 * Work out which arguments are applicable to which parameters.
 */
Requisition.prototype._assign = function(args) {
  // See comment in _split. Avoid multiple updates
  var noArgUp = { internal: true };

  this._unassigned = [];

  if (!this.commandAssignment.value) {
    return this._addUnassignedArgs(args);
  }

  if (args.length === 0) {
    this._setBlankArguments();
    return RESOLVED;
  }

  // Create an error if the command does not take parameters, but we have
  // been given them ...
  if (this.assignmentCount === 0) {
    return this._addUnassignedArgs(args);
  }

  // Special case: if there is only 1 parameter, and that's of type
  // text, then we put all the params into the first param
  if (this.assignmentCount === 1) {
    var assignment = this.getAssignment(0);
    if (assignment.param.type.name === 'string') {
      var arg = (args.length === 1) ? args[0] : new MergedArgument(args);
      return this.setAssignment(assignment, arg, noArgUp);
    }
  }

  // Positional arguments can still be specified by name, but if they are
  // then we need to ignore them when working them out positionally
  var unassignedParams = this.getParameterNames();

  // We collect the arguments used in arrays here before assigning
  var arrayArgs = {};

  // Extract all the named parameters
  var assignments = this.getAssignments(false);
  var namedDone = util.promiseEach(assignments, function(assignment) {
    // Loop over the arguments
    // Using while rather than loop because we remove args as we go
    var i = 0;
    while (i < args.length) {
      if (!assignment.param.isKnownAs(args[i].text)) {
        // Skip this parameter and handle as a positional parameter
        i++;
        continue;
      }

      var arg = args.splice(i, 1)[0];
      /* jshint loopfunc:true */
      unassignedParams = unassignedParams.filter(function(test) {
        return test !== assignment.param.name;
      });

      // boolean parameters don't have values, default to false
      if (assignment.param.type.name === 'boolean') {
        arg = new TrueNamedArgument(arg);
      }
      else {
        var valueArg = null;
        if (i + 1 <= args.length) {
          valueArg = args.splice(i, 1)[0];
        }
        arg = new NamedArgument(arg, valueArg);
      }

      if (assignment.param.type.name === 'array') {
        var arrayArg = arrayArgs[assignment.param.name];
        if (!arrayArg) {
          arrayArg = new ArrayArgument();
          arrayArgs[assignment.param.name] = arrayArg;
        }
        arrayArg.addArgument(arg);
        return RESOLVED;
      }
      else {
        if (assignment.arg.type === 'BlankArgument') {
          return this.setAssignment(assignment, arg, noArgUp);
        }
        else {
          return this._addUnassignedArgs(arg.getArgs());
        }
      }
    }
  }, this);

  // What's left are positional parameters: assign in order
  var positionalDone = namedDone.then(function() {
    return util.promiseEach(unassignedParams, function(name) {
      var assignment = this.getAssignment(name);

      // If not set positionally, and we can't set it non-positionally,
      // we have to default it to prevent previous values surviving
      if (!assignment.param.isPositionalAllowed) {
        this._setBlankAssignment(assignment);
        return RESOLVED;
      }

      // If this is a positional array argument, then it swallows the
      // rest of the arguments.
      if (assignment.param.type.name === 'array') {
        var arrayArg = arrayArgs[assignment.param.name];
        if (!arrayArg) {
          arrayArg = new ArrayArgument();
          arrayArgs[assignment.param.name] = arrayArg;
        }
        arrayArg.addArguments(args);
        args = [];
        // The actual assignment to the array parameter is done below
        return RESOLVED;
      }

      // Set assignment to defaults if there are no more arguments
      if (args.length === 0) {
        this._setBlankAssignment(assignment);
        return RESOLVED;
      }

      var arg = args.splice(0, 1)[0];
      // --foo and -f are named parameters, -4 is a number. So '-' is either
      // the start of a named parameter or a number depending on the context
      var isIncompleteName = assignment.param.type.name === 'number' ?
          /-[-a-zA-Z_]/.test(arg.text) :
          arg.text.charAt(0) === '-';

      if (isIncompleteName) {
        this._unassigned.push(new UnassignedAssignment(this, arg));
        return RESOLVED;
      }
      else {
        return this.setAssignment(assignment, arg, noArgUp);
      }
    }, this);
  }.bind(this));

  // Now we need to assign the array argument (if any)
  var arrayDone = positionalDone.then(function() {
    return util.promiseEach(Object.keys(arrayArgs), function(name) {
      var assignment = this.getAssignment(name);
      return this.setAssignment(assignment, arrayArgs[name], noArgUp);
    }, this);
  }.bind(this));

  // What's left is can't be assigned, but we need to officially unassign them
  return arrayDone.then(function() {
    return this._addUnassignedArgs(args);
  }.bind(this));
};

/**
 * Entry point for keyboard accelerators or anything else that wants to execute
 * a command.
 * @param options Object describing how the execution should be handled.
 * (optional). Contains some of the following properties:
 * - hidden (boolean, default=false) Should the output be hidden from the
 *   commandOutputManager for this requisition
 * - command/args A fast shortcut to executing a known command with a known
 *   set of parsed arguments.
 */
Requisition.prototype.exec = function(options) {
  var command = null;
  var args = null;
  var hidden = false;

  if (options) {
    if (options.hidden) {
      hidden = true;
    }

    if (options.command != null) {
      // Fast track by looking up the command directly since passed args
      // means there is no command line to parse.
      command = this.system.commands.get(options.command);
      if (!command) {
        console.error('Command not found: ' + options.command);
      }
      args = options.args;
    }
  }

  if (!command) {
    command = this.commandAssignment.value;
    args = this.getArgsObject();
  }

  // Display JavaScript input without the initial { or closing }
  var typed = this.toString();
  if (evalCmd.isCommandRegexp.test(typed)) {
    typed = typed.replace(evalCmd.isCommandRegexp, '');
    // Bug 717763: What if the JavaScript naturally ends with a }?
    typed = typed.replace(/\s*}\s*$/, '');
  }

  var output = new Output({
    command: command,
    args: args,
    typed: typed,
    canonical: this.toCanonicalString(),
    hidden: hidden
  });

  this.commandOutputManager.onOutput({ output: output });

  var onDone = function(data) {
    output.complete(data, false);
    return output;
  };

  var onError = function(data, ex) {
    if (logErrors) {
      if (ex != null) {
        util.errorHandler(ex);
      }
      else {
        console.error(data);
      }
    }

    if (data != null && typeof data === 'string') {
      data = data.replace(/^Protocol error: /, ''); // Temp fix for bug 1035296
    }

    data = (data != null && data.isTypedData) ? data : {
      isTypedData: true,
      data: data,
      type: 'error'
    };
    output.complete(data, true);
    return output;
  };

  if (this.status !== Status.VALID) {
    var ex = new Error(this.getStatusMessage());
    // We only reject a call to exec if GCLI breaks. Errors with commands are
    // exposed in the 'error' status of the Output object
    return Promise.resolve(onError(ex)).then(function(output) {
      this.clear();
      return output;
    }.bind(this));
  }
  else {
    try {
      return host.exec(function() {
        return command.exec(args, this.executionContext);
      }.bind(this)).then(onDone, onError);
    }
    catch (ex) {
      var data = (typeof ex.message === 'string' && ex.stack != null) ?
                 ex.message : ex;
      return Promise.resolve(onError(data, ex));
    }
    finally {
      this.clear();
    }
  }
};

/**
 * Designed to be called from context.updateExec(). Acts just like updateExec()
 * except that it also calls onExternalUpdate() to inform the UI of an
 * unexpected change to the current command.
 */
Requisition.prototype._contextUpdateExec = function(typed, options) {
  var reqOpts = {
    document: this.document,
    environment: this.environment
  };
  var child = new Requisition(this.system, reqOpts);
  return child.updateExec(typed, options).then(function(reply) {
    child.destroy();
    return reply;
  }.bind(child));
};

/**
 * A shortcut for calling update, resolving the promise and then exec.
 * @param input The string to execute
 * @param options Passed to exec
 * @return A promise of an output object
 */
Requisition.prototype.updateExec = function(input, options) {
  return this.update(input).then(function() {
    return this.exec(options);
  }.bind(this));
};

exports.Requisition = Requisition;

/**
 * A simple object to hold information about the output of a command
 */
function Output(options) {
  options = options || {};
  this.command = options.command || '';
  this.args = options.args || {};
  this.typed = options.typed || '';
  this.canonical = options.canonical || '';
  this.hidden = options.hidden === true ? true : false;

  this.type = undefined;
  this.data = undefined;
  this.completed = false;
  this.error = false;
  this.start = new Date();

  this.promise = new Promise(function(resolve, reject) {
    this._resolve = resolve;
  }.bind(this));
}

/**
 * Called when there is data to display, and the command has finished executing
 * See changed() for details on parameters.
 */
Output.prototype.complete = function(data, error) {
  this.end = new Date();
  this.completed = true;
  this.error = error;

  if (data != null && data.isTypedData) {
    this.data = data.data;
    this.type = data.type;
  }
  else {
    this.data = data;
    this.type = this.command.returnType;
    if (this.type == null) {
      this.type = (this.data == null) ? 'undefined' : typeof this.data;
    }
  }

  if (this.type === 'object') {
    throw new Error('No type from output of ' + this.typed);
  }

  this._resolve();
};

/**
 * Call converters.convert using the data in this Output object
 */
Output.prototype.convert = function(type, conversionContext) {
  var converters = conversionContext.system.converters;
  return converters.convert(this.data, this.type, type, conversionContext);
};

Output.prototype.toJson = function() {
  // Exceptions don't stringify, so we try a bit harder
  var data = this.data;
  if (this.error && JSON.stringify(this.data) === '{}') {
    data = {
      columnNumber: data.columnNumber,
      fileName: data.fileName,
      lineNumber: data.lineNumber,
      message: data.message,
      stack: data.stack
    };
  }

  return {
    typed: this.typed,
    type: this.type,
    data: data,
    isError: this.error
  };
};

exports.Output = Output;
