/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsoptparse.h"

#include <ctype.h>
#include <stdarg.h>

#include "jsutil.h"

using namespace js;
using namespace js::cli;
using namespace js::cli::detail;

const char OptionParser::prognameMeta[] = "{progname}";

#define OPTION_CONVERT_IMPL(__cls) \
    bool \
    Option::is##__cls##Option() const \
    { \
        return kind == OptionKind##__cls; \
    } \
    __cls##Option * \
    Option::as##__cls##Option() \
    { \
        JS_ASSERT(is##__cls##Option()); \
        return static_cast<__cls##Option *>(this); \
    } \
    const __cls##Option * \
    Option::as##__cls##Option() const \
    { \
        return const_cast<Option *>(this)->as##__cls##Option(); \
    }

ValuedOption *
Option::asValued()
{
    JS_ASSERT(isValued());
    return static_cast<ValuedOption *>(this);
}

const ValuedOption *
Option::asValued() const
{
    return const_cast<Option *>(this)->asValued();
}

OPTION_CONVERT_IMPL(Bool)
OPTION_CONVERT_IMPL(String)
OPTION_CONVERT_IMPL(Int)
OPTION_CONVERT_IMPL(MultiString)

void
OptionParser::setArgTerminatesOptions(const char *name, bool enabled)
{
    findArgument(name)->setTerminatesOptions(enabled);
}

OptionParser::Result
OptionParser::error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputs("\n\n", stderr);
    return ParseError;
}

/* Quick and dirty paragraph printer. */
static void
PrintParagraph(const char *text, unsigned startColno, const unsigned limitColno, bool padFirstLine)
{
    unsigned colno = startColno;
    const char *it = text;

    if (padFirstLine)
        printf("%*s", startColno, "");

    while (*it != '\0') {
        JS_ASSERT(!isspace(*it));

        /* Delimit the current token. */
        const char *limit = it;
        while (!isspace(*limit) && *limit != '\0')
            ++limit;

        /*
         * If the current token is longer than the available number of columns,
         * then make a line break before printing the token.
         */
        JS_ASSERT(limit - it > 0);
        size_t tokLen = limit - it;
        JS_ASSERT(tokLen);
        if (tokLen + colno >= limitColno) {
            printf("\n%*s%.*s", startColno, "", int(tokLen), it);
            colno = startColno + tokLen;
        } else {
            printf("%.*s", int(tokLen), it);
            colno += tokLen;
        }

        switch (*limit) {
          case '\0':
            return;
          case ' ':
            putchar(' ');
            colno += 1;
            it = limit;
            while (*it == ' ')
                ++it;
            break;
          case '\n':
            /* |text| wants to force a newline here. */
            printf("\n%*s", startColno, "");
            colno = startColno;
            it = limit + 1;
            /* Could also have line-leading spaces. */
            while (*it == ' ') {
                putchar(' ');
                ++colno;
                ++it;
            }
            break;
          default:
            JS_NOT_REACHED("unhandled token splitting character in text");
        }
    }
}

static const char *
OptionFlagsToFormatInfo(char shortflag, bool isValued, size_t *length)
{
    static const char *fmt[4] = { "  -%c --%s ",
                                  "  --%s ",
                                  "  -%c --%s=%s ",
                                  "  --%s=%s " };

    /* How mny chars w/o longflag? */
    size_t lengths[4] = { strlen(fmt[0]) - 3,
                          strlen(fmt[1]) - 3,
                          strlen(fmt[2]) - 5,
                          strlen(fmt[3]) - 5 };
    int index = isValued ? 2 : 0;
    if (!shortflag)
        index++;

    *length = lengths[index];
    return fmt[index];
}

OptionParser::Result
OptionParser::printHelp(const char *progname)
{
    const char *prefixEnd = strstr(usage, prognameMeta);
    if (prefixEnd) {
        printf("%.*s%s%s\n", int(prefixEnd - usage), usage, progname,
               prefixEnd + sizeof(prognameMeta) - 1);
    } else {
        puts(usage);
    }

    if (descr) {
        putchar('\n');
        PrintParagraph(descr, 2, descrWidth, true);
        putchar('\n');
    }

    if (ver)
        printf("\nVersion: %s\n\n", ver);

    if (!arguments.empty()) {
        printf("Arguments:\n");

        static const char fmt[] = "  %s ";
        size_t fmtChars = sizeof(fmt) - 2;
        size_t lhsLen = 0;
        for (Option **it = arguments.begin(), **end = arguments.end(); it != end; ++it)
            lhsLen = Max(lhsLen, strlen((*it)->longflag) + fmtChars);

        for (Option **it = arguments.begin(), **end = arguments.end(); it != end; ++it) {
            Option *arg = *it;
            size_t chars = printf(fmt, arg->longflag);
            for (; chars < lhsLen; ++chars)
                putchar(' ');
            PrintParagraph(arg->help, lhsLen, helpWidth, false);
            putchar('\n');
        }
        putchar('\n');
    }

    if (!options.empty()) {
        printf("Options:\n");

        /* Calculate sizes for column alignment. */
        size_t lhsLen = 0;
        for (Option **it = options.begin(), **end = options.end(); it != end; ++it) {
            Option *opt = *it;
            size_t longflagLen = strlen(opt->longflag);

            size_t fmtLen;
            OptionFlagsToFormatInfo(opt->shortflag, opt->isValued(), &fmtLen);

            size_t len = fmtLen + longflagLen;
            if (opt->isValued())
                len += strlen(opt->asValued()->metavar);
            lhsLen = Max(lhsLen, len);
        }

        /* Print option help text. */
        for (Option **it = options.begin(), **end = options.end(); it != end; ++it) {
            Option *opt = *it;
            size_t fmtLen;
            const char *fmt = OptionFlagsToFormatInfo(opt->shortflag, opt->isValued(), &fmtLen);
            size_t chars;
            if (opt->isValued()) {
                if (opt->shortflag)
                    chars = printf(fmt, opt->shortflag, opt->longflag, opt->asValued()->metavar);
                else
                    chars = printf(fmt, opt->longflag, opt->asValued()->metavar);
            } else {
                if (opt->shortflag)
                    chars = printf(fmt, opt->shortflag, opt->longflag);
                else
                    chars = printf(fmt, opt->longflag);
            }
            for (; chars < lhsLen; ++chars)
                putchar(' ');
            PrintParagraph(opt->help, lhsLen, helpWidth, false);
            putchar('\n');
        }
    }

    return ParseHelp;
}

OptionParser::Result
OptionParser::extractValue(size_t argc, char **argv, size_t *i, char **value)
{
    JS_ASSERT(*i < argc);
    char *eq = strchr(argv[*i], '=');
    if (eq) {
        *value = eq + 1;
        if (value[0] == '\0')
            return error("A value is required for option %.*s", eq - argv[*i], argv[*i]);
        return Okay;
    }

    if (argc == *i + 1)
        return error("Expected a value for option %s", argv[*i]);

    *i += 1;
    *value = argv[*i];
    return Okay;
}

OptionParser::Result
OptionParser::handleOption(Option *opt, size_t argc, char **argv, size_t *i, bool *optionsAllowed)
{
    if (opt->getTerminatesOptions())
        *optionsAllowed = false;

    switch (opt->kind) {
      case OptionKindBool:
      {
        if (opt == &helpOption)
            return printHelp(argv[0]);
        opt->asBoolOption()->value = true;
        return Okay;
      }
      /*
       * Valued options are allowed to specify their values either via
       * successive arguments or a single --longflag=value argument.
       */
      case OptionKindString:
      {
        char *value = NULL;
        if (Result r = extractValue(argc, argv, i, &value))
            return r;
        opt->asStringOption()->value = value;
        return Okay;
      }
      case OptionKindInt:
      {
        char *value = NULL;
        if (Result r = extractValue(argc, argv, i, &value))
            return r;
        opt->asIntOption()->value = atoi(value);
        return Okay;
      }
      case OptionKindMultiString:
      {
        char *value = NULL;
        if (Result r = extractValue(argc, argv, i, &value))
            return r;
        StringArg arg(value, *i);
        return opt->asMultiStringOption()->strings.append(arg) ? Okay : Fail;
      }
      default:
        JS_NOT_REACHED("unhandled option kind");
        return Fail;
    }
}

OptionParser::Result
OptionParser::handleArg(size_t argc, char **argv, size_t *i, bool *optionsAllowed)
{
    if (nextArgument >= arguments.length())
        return error("Too many arguments provided");

    Option *arg = arguments[nextArgument];

    if (arg->getTerminatesOptions())
        *optionsAllowed = false;

    switch (arg->kind) {
      case OptionKindString:
        arg->asStringOption()->value = argv[*i];
        nextArgument += 1;
        return Okay;
      case OptionKindMultiString:
      {
        /* Don't advance the next argument -- there can only be one (final) variadic argument. */
        StringArg value(argv[*i], *i);
        return arg->asMultiStringOption()->strings.append(value) ? Okay : Fail;
      }
      default:
        JS_NOT_REACHED("unhandled argument kind");
        return Fail;
    }
}

OptionParser::Result
OptionParser::parseArgs(int inputArgc, char **argv)
{
    JS_ASSERT(inputArgc >= 0);
    size_t argc = inputArgc;
    /* Permit a "no more options" capability, like |--| offers in many shell interfaces. */
    bool optionsAllowed = true;

    for (size_t i = 1; i < argc; ++i) {
        char *arg = argv[i];
        Result r;
        /* Note: solo dash option is actually a 'stdin' argument. */
        if (arg[0] == '-' && arg[1] != '\0' && optionsAllowed) {
            /* Option. */
            Option *opt;
            if (arg[1] == '-') {
                /* Long option. */
                opt = findOption(arg + 2);
                if (!opt)
                    return error("Invalid long option: %s", arg);
            } else {
                /* Short option */
                if (arg[2] != '\0')
                    return error("Short option followed by junk: %s", arg);
                opt = findOption(arg[1]);
                if (!opt)
                    return error("Invalid short option: %s", arg);
            }

            r = handleOption(opt, argc, argv, &i, &optionsAllowed);
        } else {
            /* Argument. */
            r = handleArg(argc, argv, &i, &optionsAllowed);
        }

        switch (r) {
          case Okay:
            break;
          default:
            return r;
        }
    }
    return Okay;
}

void
OptionParser::setHelpOption(char shortflag, const char *longflag, const char *help)
{
    helpOption.setFlagInfo(shortflag, longflag, help);
}

bool
OptionParser::getHelpOption() const
{
    return helpOption.value;
}

bool
OptionParser::getBoolOption(char shortflag) const
{
    return findOption(shortflag)->asBoolOption()->value;
}

int
OptionParser::getIntOption(char shortflag) const
{
    return findOption(shortflag)->asIntOption()->value;
}

const char *
OptionParser::getStringOption(char shortflag) const
{
    return findOption(shortflag)->asStringOption()->value;
}

MultiStringRange
OptionParser::getMultiStringOption(char shortflag) const
{
    const MultiStringOption *mso = findOption(shortflag)->asMultiStringOption();
    return MultiStringRange(mso->strings.begin(), mso->strings.end());
}

bool
OptionParser::getBoolOption(const char *longflag) const
{
    return findOption(longflag)->asBoolOption()->value;
}

int
OptionParser::getIntOption(const char *longflag) const
{
    return findOption(longflag)->asIntOption()->value;
}

const char *
OptionParser::getStringOption(const char *longflag) const
{
    return findOption(longflag)->asStringOption()->value;
}

MultiStringRange
OptionParser::getMultiStringOption(const char *longflag) const
{
    const MultiStringOption *mso = findOption(longflag)->asMultiStringOption();
    return MultiStringRange(mso->strings.begin(), mso->strings.end());
}

OptionParser::~OptionParser()
{
    for (Option **it = options.begin(), **end = options.end(); it != end; ++it)
        js_delete<Option>(*it);
    for (Option **it = arguments.begin(), **end = arguments.end(); it != end; ++it)
        js_delete<Option>(*it);
}

Option *
OptionParser::findOption(char shortflag)
{
    for (Option **it = options.begin(), **end = options.end(); it != end; ++it) {
        if ((*it)->shortflag == shortflag)
            return *it;
    }

    return helpOption.shortflag == shortflag ? &helpOption : NULL;
}

const Option *
OptionParser::findOption(char shortflag) const
{
    return const_cast<OptionParser *>(this)->findOption(shortflag);
}

Option *
OptionParser::findOption(const char *longflag)
{
    for (Option **it = options.begin(), **end = options.end(); it != end; ++it) {
        const char *target = (*it)->longflag;
        if ((*it)->isValued()) {
            size_t targetLen = strlen(target);
            /* Permit a trailing equals sign on the longflag argument. */
            for (size_t i = 0; i < targetLen; ++i) {
                if (longflag[i] == '\0' || longflag[i] != target[i])
                    goto no_match;
            }
            if (longflag[targetLen] == '\0' || longflag[targetLen] == '=')
                return *it;
        } else {
            if (strcmp(target, longflag) == 0)
                return *it;
        }
  no_match:;
    }

    return strcmp(helpOption.longflag, longflag) ? NULL : &helpOption;
}

const Option *
OptionParser::findOption(const char *longflag) const
{
    return const_cast<OptionParser *>(this)->findOption(longflag);
}

/* Argument accessors */

Option *
OptionParser::findArgument(const char *name)
{
    for (Option **it = arguments.begin(), **end = arguments.end(); it != end; ++it) {
        const char *target = (*it)->longflag;
        if (strcmp(target, name) == 0)
            return *it;
    }
    return NULL;
}

const Option *
OptionParser::findArgument(const char *name) const
{
    return const_cast<OptionParser *>(this)->findArgument(name);
}

const char *
OptionParser::getStringArg(const char *name) const
{
    return findArgument(name)->asStringOption()->value;
}

MultiStringRange
OptionParser::getMultiStringArg(const char *name) const
{
    const MultiStringOption *mso = findArgument(name)->asMultiStringOption();
    return MultiStringRange(mso->strings.begin(), mso->strings.end());
}

/* Option builders */

bool
OptionParser::addIntOption(char shortflag, const char *longflag, const char *metavar,
                           const char *help, int defaultValue)
{
    if (!options.reserve(options.length() + 1))
        return false;
    IntOption *io = js_new<IntOption>(shortflag, longflag, help, metavar, defaultValue);
    if (!io)
        return false;
    options.infallibleAppend(io);
    return true;
}

bool
OptionParser::addBoolOption(char shortflag, const char *longflag, const char *help)
{
    if (!options.reserve(options.length() + 1))
        return false;
    BoolOption *bo = js_new<BoolOption>(shortflag, longflag, help);
    if (!bo)
        return false;
    options.infallibleAppend(bo);
    return true;
}

bool
OptionParser::addStringOption(char shortflag, const char *longflag, const char *metavar,
                              const char *help)
{
    if (!options.reserve(options.length() + 1))
        return false;
    StringOption *so = js_new<StringOption>(shortflag, longflag, help, metavar);
    if (!so)
        return false;
    options.infallibleAppend(so);
    return true;
}

bool
OptionParser::addMultiStringOption(char shortflag, const char *longflag, const char *metavar,
                                   const char *help)
{
    if (!options.reserve(options.length() + 1))
        return false;
    MultiStringOption *mso = js_new<MultiStringOption>(shortflag, longflag, help, metavar);
    if (!mso)
        return false;
    options.infallibleAppend(mso);
    return true;
}

/* Argument builders */

bool
OptionParser::addOptionalStringArg(const char *name, const char *help)
{
    if (!arguments.reserve(arguments.length() + 1))
        return false;
    StringOption *so = js_new<StringOption>(1, name, help, (const char *) NULL);
    if (!so)
        return false;
    arguments.infallibleAppend(so);
    return true;
}

bool
OptionParser::addOptionalMultiStringArg(const char *name, const char *help)
{
    JS_ASSERT_IF(!arguments.empty(), !arguments.back()->isVariadic());
    if (!arguments.reserve(arguments.length() + 1))
        return false;
    MultiStringOption *mso = js_new<MultiStringOption>(1, name, help, (const char *) NULL);
    if (!mso)
        return false;
    arguments.infallibleAppend(mso);
    return true;
}
