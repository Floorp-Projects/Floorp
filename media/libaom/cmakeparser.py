# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from pyparsing import (CharsNotIn, Group, Forward, Literal, Suppress, Word,
                       QuotedString, ZeroOrMore, alphas, alphanums)
from string import Template
import re

# Grammar for CMake
comment = Literal('#') + ZeroOrMore(CharsNotIn('\n'))
quoted_argument = QuotedString('\"', '\\', multiline=True)
unquoted_argument = CharsNotIn('\n ()#\"\\')
argument = quoted_argument | unquoted_argument | Suppress(comment)
arguments = Forward()
arguments << (argument | (Literal('(') + ZeroOrMore(arguments) + Literal(')')))
identifier = Word(alphas, alphanums+'_')
command = Group(identifier + Literal('(') + ZeroOrMore(arguments) + Literal(')'))
file_elements = command | Suppress(comment)
cmake = ZeroOrMore(file_elements)


def extract_arguments(parsed):
    """Extract the command arguments skipping the parentheses"""
    return parsed[2:len(parsed) - 1]


def match_block(command, parsed, start):
    """Find the end of block starting with the command"""
    depth = 0
    end = start + 1
    endcommand = 'end' + command
    while parsed[end][0] != endcommand or depth > 0:
        if parsed[end][0] == command:
            depth += 1
        elif parsed[end][0] == endcommand:
            depth -= 1
        end = end + 1
        if end == len(parsed):
            print('error: eof when trying to match block statement: %s'
                  % parsed[start])
    return end


def parse_if(parsed, start):
    """Parse if/elseif/else/endif into a list of conditions and commands"""
    depth = 0
    conditions = []
    condition = [extract_arguments(parsed[start])]
    start = start + 1
    end = start

    while parsed[end][0] != 'endif' or depth > 0:
        command = parsed[end][0]
        if command == 'if':
            depth += 1
        elif command == 'else' and depth == 0:
            condition.append(parsed[start:end])
            conditions.append(condition)
            start = end + 1
            condition = [['TRUE']]
        elif command == 'elseif' and depth == 0:
            condition.append(parsed[start:end])
            conditions.append(condition)
            condition = [extract_arguments(parsed[end])]
            start = end + 1
        elif command == 'endif':
            depth -= 1
        end = end + 1
        if end == len(parsed):
            print('error: eof when trying to match if statement: %s'
                  % parsed[start])
    condition.append(parsed[start:end])
    conditions.append(condition)
    return end, conditions


def substs(variables, values):
    """Substitute variables into values"""
    new_values = []
    for value in values:
        t = Template(value)
        new_value = t.safe_substitute(variables)

        # Safe substitute leaves unrecognized variables in place.
        # We replace them with the empty string.
        new_values.append(re.sub('\$\{\w+\}', '', new_value))
    return new_values


def evaluate(variables, cache_variables, parsed):
    """Evaluate a list of parsed commands, returning sources to build"""
    i = 0
    sources = []
    while i < len(parsed):
        command = parsed[i][0]
        arguments = substs(variables, extract_arguments(parsed[i]))

        if command == 'foreach':
            end = match_block(command, parsed, i)
            for argument in arguments[1:]:
                # ; is also a valid divider, why have one when you can have two?
                argument = argument.replace(';', ' ')
                for value in argument.split():
                    variables[arguments[0]] = value
                    cont_eval, new_sources = evaluate(variables, cache_variables,
                                                      parsed[i+1:end])
                    sources.extend(new_sources)
                    if not cont_eval:
                        return cont_eval, sources
        elif command == 'function':
            # for now we just execute functions inline at point of declaration
            # as this is sufficient to build libaom
            pass
        elif command == 'if':
            i, conditions = parse_if(parsed, i)
            for condition in conditions:
                if evaluate_boolean(variables, condition[0]):
                    cont_eval, new_sources = evaluate(variables,
                                                      cache_variables,
                                                      condition[1])
                    sources.extend(new_sources)
                    if not cont_eval:
                        return cont_eval, sources
                    break
        elif command == 'include':
            if arguments:
                try:
                    print('including: %s' % arguments[0])
                    sources.extend(parse(variables, cache_variables, arguments[0]))
                except IOError:
                    print('warning: could not include: %s' % arguments[0])
        elif command == 'list':
            try:
                action = arguments[0]
                variable = arguments[1]
                values = arguments[2:]
                if action == 'APPEND':
                    if not variables.has_key(variable):
                        variables[variable] = ' '.join(values)
                    else:
                        variables[variable] += ' ' + ' '.join(values)
            except (IndexError, KeyError):
                pass
        elif command == 'option':
            variable = arguments[0]
            value = arguments[2]
            # Allow options to be override without changing CMake files
            if not variables.has_key(variable):
                variables[variable] = value
        elif command == 'return':
            return False, sources
        elif command == 'set':
            variable = arguments[0]
            values = arguments[1:]
            # CACHE variables are not set if already present
            try:
                cache = values.index('CACHE')
                values = values[0:cache]
                if not variables.has_key(variable):
                    variables[variable] = ' '.join(values)
                cache_variables.append(variable)
            except ValueError:
                variables[variable] = ' '.join(values)
        elif command == 'add_asm_library':
            try:
                sources.extend(variables[arguments[1]].split(' '))
            except (IndexError, KeyError):
                pass
        elif command == 'add_intrinsics_object_library':
            try:
                sources.extend(variables[arguments[3]].split(' '))
            except (IndexError, KeyError):
                pass
        elif command == 'add_library':
            for source in arguments[1:]:
                sources.extend(source.split(' '))
        elif command == 'target_sources':
            for source in arguments[1:]:
                sources.extend(source.split(' '))
        elif command == 'MOZDEBUG':
            print('>>>> MOZDEBUG: %s' % ' '.join(arguments))
        i += 1
    return True, sources


def evaluate_boolean(variables, arguments):
    """Evaluate a boolean expression"""
    if not arguments:
        return False

    argument = arguments[0]

    if argument == 'NOT':
        return not evaluate_boolean(variables, arguments[1:])

    if argument == '(':
        i = 0
        depth = 1
        while depth > 0 and i < len(arguments):
            i += 1
            if arguments[i] == '(':
                depth += 1
            if arguments[i] == ')':
                depth -= 1
        return evaluate_boolean(variables, arguments[1:i])

    def evaluate_constant(argument):
        try:
            as_int = int(argument)
            if as_int != 0:
                return True
            else:
                return False
        except ValueError:
            upper = argument.upper()
            if upper in ['ON', 'YES', 'TRUE', 'Y']:
                return True
            elif upper in ['OFF', 'NO', 'FALSE', 'N', 'IGNORE', '', 'NOTFOUND']:
                return False
            elif upper.endswith('-NOTFOUND'):
                return False
        return None

    def lookup_variable(argument):
        # If statements can have old-style variables which are not demarcated
        # like ${VARIABLE}. Attempt to look up the variable both ways.
        try:
            if re.search('\$\{\w+\}', argument):
                try:
                    t = Template(argument)
                    value = t.substitute(variables)
                    try:
                        # Attempt an old-style variable lookup with the
                        # substituted value.
                        return variables[value]
                    except KeyError:
                        return value
                except ValueError:
                    # TODO: CMake supports nesting, e.g. ${${foo}}
                    return None
            else:
                return variables[argument]
        except KeyError:
            return None

    lhs = lookup_variable(argument)
    if lhs is None:
        # variable resolution failed, treat as string
        lhs = argument

    if len(arguments) > 1:
        op = arguments[1]
        if op == 'AND':
            return evaluate_constant(lhs) and evaluate_boolean(variables, arguments[2:])
        elif op == 'MATCHES':
            rhs = lookup_variable(arguments[2])
            if not rhs:
                rhs = arguments[2]
            return not re.match(rhs, lhs) is None
        elif op == 'OR':
            return evaluate_constant(lhs) or evaluate_boolean(variables, arguments[2:])
        elif op == 'STREQUAL':
            rhs = lookup_variable(arguments[2])
            if not rhs:
                rhs = arguments[2]
            return lhs == rhs
    else:
        lhs = evaluate_constant(lhs)
        if lhs is None:
            lhs = lookup_variable(argument)

    return lhs


def parse(variables, cache_variables, filename):
    parsed = cmake.parseFile(filename)
    cont_eval, sources = evaluate(variables, cache_variables, parsed)
    return sources
