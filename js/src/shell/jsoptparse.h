/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef shell_jsoptparse_h
#define shell_jsoptparse_h

#include <stdio.h>

#include "js/AllocPolicy.h"
#include "js/Vector.h"

namespace js {
namespace cli {

namespace detail {

struct BoolOption;
struct MultiStringOption;
struct ValuedOption;
struct StringOption;
struct IntOption;

enum OptionKind {
  OptionKindBool,
  OptionKindString,
  OptionKindInt,
  OptionKindMultiString,
  OptionKindInvalid
};

struct Option {
  const char* longflag;
  const char* help;
  OptionKind kind;
  char shortflag;
  bool terminatesOptions;

  Option(OptionKind kind, char shortflag, const char* longflag,
         const char* help)
      : longflag(longflag),
        help(help),
        kind(kind),
        shortflag(shortflag),
        terminatesOptions(false) {}

  virtual ~Option() = 0;

  void setTerminatesOptions(bool enabled) { terminatesOptions = enabled; }
  bool getTerminatesOptions() const { return terminatesOptions; }

  virtual bool isValued() const { return false; }

  /* Only some valued options are variadic (like MultiStringOptions). */
  virtual bool isVariadic() const { return false; }

  /*
   * For arguments, the shortflag field is used to indicate whether the
   * argument is optional.
   */
  bool isOptional() { return shortflag; }

  void setFlagInfo(char shortflag, const char* longflag, const char* help) {
    this->shortflag = shortflag;
    this->longflag = longflag;
    this->help = help;
  }

  ValuedOption* asValued();
  const ValuedOption* asValued() const;

#define OPTION_CONVERT_DECL(__cls)    \
  bool is##__cls##Option() const;     \
  __cls##Option* as##__cls##Option(); \
  const __cls##Option* as##__cls##Option() const;

  OPTION_CONVERT_DECL(Bool)
  OPTION_CONVERT_DECL(String)
  OPTION_CONVERT_DECL(Int)
  OPTION_CONVERT_DECL(MultiString)
};

inline Option::~Option() {}

struct BoolOption : public Option {
  size_t argno;
  bool value;

  BoolOption(char shortflag, const char* longflag, const char* help)
      : Option(OptionKindBool, shortflag, longflag, help), value(false) {}

  virtual ~BoolOption() {}
};

struct ValuedOption : public Option {
  const char* metavar;

  ValuedOption(OptionKind kind, char shortflag, const char* longflag,
               const char* help, const char* metavar)
      : Option(kind, shortflag, longflag, help), metavar(metavar) {}

  virtual ~ValuedOption() = 0;
  virtual bool isValued() const override { return true; }
};

inline ValuedOption::~ValuedOption() {}

struct StringOption : public ValuedOption {
  const char* value;

  StringOption(char shortflag, const char* longflag, const char* help,
               const char* metavar)
      : ValuedOption(OptionKindString, shortflag, longflag, help, metavar),
        value(nullptr) {}

  virtual ~StringOption() {}
};

struct IntOption : public ValuedOption {
  int value;

  IntOption(char shortflag, const char* longflag, const char* help,
            const char* metavar, int defaultValue)
      : ValuedOption(OptionKindInt, shortflag, longflag, help, metavar),
        value(defaultValue) {}

  virtual ~IntOption() {}
};

struct StringArg {
  char* value;
  size_t argno;

  StringArg(char* value, size_t argno) : value(value), argno(argno) {}
};

struct MultiStringOption : public ValuedOption {
  Vector<StringArg, 0, SystemAllocPolicy> strings;

  MultiStringOption(char shortflag, const char* longflag, const char* help,
                    const char* metavar)
      : ValuedOption(OptionKindMultiString, shortflag, longflag, help,
                     metavar) {}

  virtual ~MultiStringOption() {}

  virtual bool isVariadic() const override { return true; }
};

} /* namespace detail */

class MultiStringRange {
  typedef detail::StringArg StringArg;
  const StringArg* cur;
  const StringArg* end;

 public:
  explicit MultiStringRange(const StringArg* cur, const StringArg* end)
      : cur(cur), end(end) {
    MOZ_ASSERT(end - cur >= 0);
  }

  bool empty() const { return cur == end; }
  void popFront() {
    MOZ_ASSERT(!empty());
    ++cur;
  }
  char* front() const {
    MOZ_ASSERT(!empty());
    return cur->value;
  }
  size_t argno() const {
    MOZ_ASSERT(!empty());
    return cur->argno;
  }
};

/*
 * Builder for describing a command line interface and parsing the resulting
 * specification.
 *
 * - A multi-option is an option that can appear multiple times and still
 *   parse as valid command line arguments.
 * - An "optional argument" is supported for backwards compatibility with prior
 *   command line interface usage. Once one optional argument has been added,
 *   *only* optional arguments may be added.
 */
class OptionParser {
 public:
  enum Result {
    Okay = 0,
    Fail,       /* As in, allocation fail. */
    ParseError, /* Successfully parsed but with an error. */
    EarlyExit   /* Successfully parsed but exits the program,
                 * for example with --help and --version. */
  };

 private:
  typedef Vector<detail::Option*, 0, SystemAllocPolicy> Options;
  typedef detail::Option Option;
  typedef detail::BoolOption BoolOption;

  Options options;
  Options arguments;
  BoolOption helpOption;
  BoolOption versionOption;
  const char* usage;
  const char* version;
  const char* descr;
  size_t descrWidth;
  size_t helpWidth;
  size_t nextArgument;

  // If '--' is passed, all remaining arguments should be interpreted as the
  // argument at index 'restArgument'. Defaults to the next unassigned
  // argument.
  int restArgument;

  static const char prognameMeta[];

  Option* findOption(char shortflag);
  const Option* findOption(char shortflag) const;
  Option* findOption(const char* longflag);
  const Option* findOption(const char* longflag) const;
  int findArgumentIndex(const char* name) const;
  Option* findArgument(const char* name);
  const Option* findArgument(const char* name) const;

  Result error(const char* fmt, ...) MOZ_FORMAT_PRINTF(2, 3);
  Result extractValue(size_t argc, char** argv, size_t* i, char** value);
  Result handleArg(size_t argc, char** argv, size_t* i, bool* optsAllowed);
  Result handleOption(Option* opt, size_t argc, char** argv, size_t* i,
                      bool* optsAllowed);

 public:
  explicit OptionParser(const char* usage)
      : helpOption('h', "help", "Display help information"),
        versionOption('v', "version", "Display version information and exit"),
        usage(usage),
        version(nullptr),
        descr(nullptr),
        descrWidth(80),
        helpWidth(80),
        nextArgument(0),
        restArgument(-1) {}

  ~OptionParser();

  Result parseArgs(int argc, char** argv);
  Result printHelp(const char* progname);
  Result printVersion();

  /* Metadata */

  void setVersion(const char* v) { version = v; }
  void setHelpWidth(size_t width) { helpWidth = width; }
  void setDescriptionWidth(size_t width) { descrWidth = width; }
  void setDescription(const char* description) { descr = description; }
  void setHelpOption(char shortflag, const char* longflag, const char* help);
  void setArgTerminatesOptions(const char* name, bool enabled);
  void setArgCapturesRest(const char* name);

  /* Arguments: no further arguments may be added after a variadic argument. */

  bool addOptionalStringArg(const char* name, const char* help);
  bool addOptionalMultiStringArg(const char* name, const char* help);

  const char* getStringArg(const char* name) const;
  MultiStringRange getMultiStringArg(const char* name) const;

  /* Options */

  bool addBoolOption(char shortflag, const char* longflag, const char* help);
  bool addStringOption(char shortflag, const char* longflag, const char* help,
                       const char* metavar);
  bool addIntOption(char shortflag, const char* longflag, const char* help,
                    const char* metavar, int defaultValue);
  bool addMultiStringOption(char shortflag, const char* longflag,
                            const char* help, const char* metavar);
  bool addOptionalVariadicArg(const char* name);

  int getIntOption(char shortflag) const;
  int getIntOption(const char* longflag) const;
  const char* getStringOption(char shortflag) const;
  const char* getStringOption(const char* longflag) const;
  bool getBoolOption(char shortflag) const;
  bool getBoolOption(const char* longflag) const;
  MultiStringRange getMultiStringOption(char shortflag) const;
  MultiStringRange getMultiStringOption(const char* longflag) const;

  /*
   * Return whether the help option was present (and thus help was already
   * displayed during parse_args).
   */
  bool getHelpOption() const;
};

} /* namespace cli */
} /* namespace js */

#endif /* shell_jsoptparse_h */
