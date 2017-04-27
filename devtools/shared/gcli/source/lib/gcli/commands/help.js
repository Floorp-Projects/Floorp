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

var l10n = require('../util/l10n');
var cli = require('../cli');

/**
 * Add an 'paramGroups' accessor to a command metadata object to sort the
 * params into groups according to the option of the param.
 */
function addParamGroups(command) {
  Object.defineProperty(command, 'paramGroups', {
    get: function() {
      var paramGroups = {};
      this.params.forEach(function(param) {
        var groupName = param.option || l10n.lookup('canonDefaultGroupName');
        if (paramGroups[groupName] == null) {
          paramGroups[groupName] = [];
        }
        paramGroups[groupName].push(param);
      });
      return paramGroups;
    },
    enumerable: true
  });
}

/**
 * Get a data block for the help_man.html/help_man.txt templates
 */
function getHelpManData(commandData, context) {
  // Filter out hidden parameters
  commandData.command.params = commandData.command.params.filter(
    param => !param.hidden
  );

  addParamGroups(commandData.command);
  commandData.subcommands.forEach(addParamGroups);

  return {
    l10n: l10n.propertyLookup,
    onclick: context.update,
    ondblclick: context.updateExec,
    describe: function(item) {
      return item.manual || item.description;
    },
    getTypeDescription: function(param) {
      var input = '';
      if (param.defaultValue === undefined) {
        input = l10n.lookup('helpManRequired');
      }
      else if (param.defaultValue === null) {
        input = l10n.lookup('helpManOptional');
      }
      else {
        // We need defaultText to work the text version of defaultValue
        input = l10n.lookup('helpManOptional');
        /*
        var val = param.type.stringify(param.defaultValue);
        input = Promise.resolve(val).then(function(defaultValue) {
          return l10n.lookupFormat('helpManDefault', [ defaultValue ]);
        }.bind(this));
        */
      }

      return Promise.resolve(input).then(defaultDescr => {
        return '(' + (param.type.name || param.type) + ', ' + defaultDescr + ')';
      });
    },
    getSynopsis: function(param) {
      var name = param.name + (param.short ? '|-' + param.short : '');
      if (param.option == null) {
        return param.defaultValue !== undefined ?
            '[' + name + ']' :
            '<' + name + '>';
      }
      else {
        return param.type === 'boolean' || param.type.name === 'boolean' ?
            '[--' + name + ']' :
            '[--' + name + ' ...]';
      }
    },
    command: commandData.command,
    subcommands: commandData.subcommands
  };
}

/**
 * Get a data block for the help_list.html/help_list.txt templates
 */
function getHelpListData(commandsData, context) {
  commandsData.commands.forEach(addParamGroups);

  var heading;
  if (commandsData.commands.length === 0) {
    heading = l10n.lookupFormat('helpListNone', [ commandsData.prefix ]);
  }
  else if (commandsData.prefix == null) {
    heading = l10n.lookup('helpListAll');
  }
  else {
    heading = l10n.lookupFormat('helpListPrefix', [ commandsData.prefix ]);
  }

  return {
    l10n: l10n.propertyLookup,
    includeIntro: commandsData.prefix == null,
    heading: heading,
    onclick: context.update,
    ondblclick: context.updateExec,
    matchingCommands: commandsData.commands
  };
}

/**
 * Create a block of data suitable to be passed to the help_list.html template
 */
function getMatchingCommands(context, prefix) {
  var commands = cli.getMapping(context).requisition.system.commands;
  var reply = commands.getAll().filter(function(command) {
    if (command.hidden) {
      return false;
    }

    if (prefix && command.name.indexOf(prefix) !== 0) {
      // Filtered out because they don't match the search
      return false;
    }
    if (!prefix && command.name.indexOf(' ') != -1) {
      // We don't show sub commands with plain 'help'
      return false;
    }
    return true;
  });

  reply.sort(function(c1, c2) {
    return c1.name.localeCompare(c2.name);
  });

  reply = reply.map(function(command) {
    return command.toJson();
  });

  return reply;
}

/**
 * Find all the sub commands of the given command
 */
function getSubCommands(context, command) {
  var commands = cli.getMapping(context).requisition.system.commands;
  var subcommands = commands.getAll().filter(function(subcommand) {
    return subcommand.name.indexOf(command.name) === 0 &&
           subcommand.name !== command.name &&
           !subcommand.hidden;
  });

  subcommands.sort(function(c1, c2) {
    return c1.name.localeCompare(c2.name);
  });

  subcommands = subcommands.map(function(subcommand) {
    return subcommand.toJson();
  });

  return subcommands;
}

var helpCss = '' +
  '.gcli-help-name {\n' +
  '  text-align: end;\n' +
  '}\n' +
  '\n' +
  '.gcli-help-arrow {\n' +
  '  color: #AAA;\n' +
  '}\n' +
  '\n' +
  '.gcli-help-description {\n' +
  '  margin: 0 20px;\n' +
  '  padding: 0;\n' +
  '}\n' +
  '\n' +
  '.gcli-help-parameter {\n' +
  '  margin: 0 30px;\n' +
  '  padding: 0;\n' +
  '}\n' +
  '\n' +
  '.gcli-help-header {\n' +
  '  margin: 10px 0 6px;\n' +
  '}\n';

exports.items = [
  {
    // 'help' command
    item: 'command',
    name: 'help',
    runAt: 'client',
    description: l10n.lookup('helpDesc'),
    manual: l10n.lookup('helpManual'),
    params: [
      {
        name: 'search',
        type: 'string',
        description: l10n.lookup('helpSearchDesc'),
        manual: l10n.lookup('helpSearchManual3'),
        defaultValue: null
      }
    ],

    exec: function(args, context) {
      var commands = cli.getMapping(context).requisition.system.commands;
      var command = commands.get(args.search);
      if (command) {
        return context.typedData('commandData', {
          command: command.toJson(),
          subcommands: getSubCommands(context, command)
        });
      }

      return context.typedData('commandsData', {
        prefix: args.search,
        commands: getMatchingCommands(context, args.search)
      });
    }
  },
  {
    // Convert a command into an HTML man page
    item: 'converter',
    from: 'commandData',
    to: 'view',
    exec: function(commandData, context) {
      return {
        html:
          '<div>\n' +
          '  <p class="gcli-help-header">\n' +
          '    ${l10n.helpManSynopsis}:\n' +
          '    <span class="gcli-out-shortcut" data-command="${command.name}"\n' +
          '        onclick="${onclick}" ondblclick="${ondblclick}">\n' +
          '      ${command.name}\n' +
          '      <span foreach="param in ${command.params}">${getSynopsis(param)} </span>\n' +
          '    </span>\n' +
          '  </p>\n' +
          '\n' +
          '  <p class="gcli-help-description">${describe(command)}</p>\n' +
          '\n' +
          '  <div if="${!command.isParent}">\n' +
          '    <div foreach="groupName in ${command.paramGroups}">\n' +
          '      <p class="gcli-help-header">${groupName}:</p>\n' +
          '      <ul class="gcli-help-parameter">\n' +
          '        <li if="${command.params.length === 0}">${l10n.helpManNone}</li>\n' +
          '        <li foreach="param in ${command.paramGroups[groupName]}">\n' +
          '          <code>${getSynopsis(param)}</code> <em>${getTypeDescription(param)}</em>\n' +
          '          <br/>\n' +
          '          ${describe(param)}\n' +
          '        </li>\n' +
          '      </ul>\n' +
          '    </div>\n' +
          '  </div>\n' +
          '\n' +
          '  <div if="${command.isParent}">\n' +
          '    <p class="gcli-help-header">${l10n.subCommands}:</p>\n' +
          '    <ul class="gcli-help-${subcommands}">\n' +
          '      <li if="${subcommands.length === 0}">${l10n.subcommandsNone}</li>\n' +
          '      <li foreach="subcommand in ${subcommands}">\n' +
          '        ${subcommand.name}: ${subcommand.description}\n' +
          '        <span class="gcli-out-shortcut" data-command="help ${subcommand.name}"\n' +
          '            onclick="${onclick}" ondblclick="${ondblclick}">\n' +
          '          help ${subcommand.name}\n' +
          '        </span>\n' +
          '      </li>\n' +
          '    </ul>\n' +
          '  </div>\n' +
          '\n' +
          '</div>\n',
        options: { allowEval: true, stack: 'commandData->view' },
        data: getHelpManData(commandData, context),
        css: helpCss,
        cssId: 'gcli-help'
      };
    }
  },
  {
    // Convert a command into a string based man page
    item: 'converter',
    from: 'commandData',
    to: 'stringView',
    exec: function(commandData, context) {
      return {
        html:
          '<div>## ${command.name}\n' +
          '\n' +
          '# ${l10n.helpManSynopsis}: ${command.name} <loop foreach="param in ${command.params}">${getSynopsis(param)} </loop>\n' +
          '\n' +
          '# ${l10n.helpManDescription}:\n' +
          '\n' +
          '${command.manual || command.description}\n' +
          '\n' +
          '<loop foreach="groupName in ${command.paramGroups}">\n' +
          '<span if="${!command.isParent}"># ${groupName}:\n' +
          '\n' +
          '<span if="${command.params.length === 0}">${l10n.helpManNone}</span><loop foreach="param in ${command.paramGroups[groupName]}">* ${param.name}: ${getTypeDescription(param)}\n' +
          '  ${param.manual || param.description}\n' +
          '</loop>\n' +
          '</span>\n' +
          '</loop>\n' +
          '\n' +
          '<span if="${command.isParent}"># ${l10n.subCommands}:</span>\n' +
          '\n' +
          '<span if="${subcommands.length === 0}">${l10n.subcommandsNone}</span>\n' +
          '<loop foreach="subcommand in ${subcommands}">* ${subcommand.name}: ${subcommand.description}\n' +
          '</loop>\n' +
          '</div>\n',
        options: { allowEval: true, stack: 'commandData->stringView' },
        data: getHelpManData(commandData, context)
      };
    }
  },
  {
    // Convert a list of commands into a formatted list
    item: 'converter',
    from: 'commandsData',
    to: 'view',
    exec: function(commandsData, context) {
      return {
        html:
          '<div>\n' +
          '  <div if="${includeIntro}">\n' +
          '    <p>${l10n.helpIntro}</p>\n' +
          '  </div>\n' +
          '\n' +
          '  <p>${heading}</p>\n' +
          '\n' +
          '  <table>\n' +
          '    <tr foreach="command in ${matchingCommands}">\n' +
          '      <td class="gcli-help-name">${command.name}</td>\n' +
          '      <td class="gcli-help-arrow">-</td>\n' +
          '      <td>\n' +
          '        ${command.description}\n' +
          '        <span class="gcli-out-shortcut"\n' +
          '            onclick="${onclick}" ondblclick="${ondblclick}"\n' +
          '            data-command="help ${command.name}">help ${command.name}</span>\n' +
          '      </td>\n' +
          '    </tr>\n' +
          '  </table>\n' +
          '</div>\n',
        options: { allowEval: true, stack: 'commandsData->view' },
        data: getHelpListData(commandsData, context),
        css: helpCss,
        cssId: 'gcli-help'
      };
    }
  },
  {
    // Convert a list of commands into a formatted list
    item: 'converter',
    from: 'commandsData',
    to: 'stringView',
    exec: function(commandsData, context) {
      return {
        html:
          '<pre><span if="${includeIntro}">## ${l10n.helpIntro}</span>\n' +
          '\n' +
          '# ${heading}\n' +
          '\n' +
          '<loop foreach="command in ${matchingCommands}">${command.name} &#x2192; ${command.description}\n' +
          '</loop></pre>',
        options: { allowEval: true, stack: 'commandsData->stringView' },
        data: getHelpListData(commandsData, context)
      };
    }
  }
];
