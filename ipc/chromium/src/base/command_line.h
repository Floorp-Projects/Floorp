// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class works with command lines: building and parsing.
// Switches can optionally have a value attached using an equals sign,
// as in "-switch=value".  Arguments that aren't prefixed with a
// switch prefix are considered "loose parameters".  Switch names are
// case-insensitive.  An argument of "--" will terminate switch
// parsing, causing everything after to be considered as loose
// parameters.

// There is a singleton read-only CommandLine that represents the command
// line that the current process was started with.  It must be initialized
// in main() (or whatever the platform's equivalent function is).

#ifndef BASE_COMMAND_LINE_H_
#define BASE_COMMAND_LINE_H_

#include "build/build_config.h"

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"

class InProcessBrowserTest;

class CommandLine {
 public:
#if defined(OS_WIN)
  // Creates a parsed version of the given command-line string.
  // The program name is assumed to be the first item in the string.
  void ParseFromString(const std::wstring& command_line);
#elif defined(OS_POSIX)
  // Initialize from an argv vector (or directly from main()'s argv).
  CommandLine(int argc, const char* const* argv);
  explicit CommandLine(const std::vector<std::string>& argv);
#endif

  // Construct a new, empty command line.
  // |program| is the name of the program to run (aka argv[0]).
  // TODO(port): should be a FilePath.
  explicit CommandLine(const std::wstring& program);

  // Initialize the current process CommandLine singleton.  On Windows,
  // ignores its arguments (we instead parse GetCommandLineW()
  // directly) because we don't trust the CRT's parsing of the command
  // line, but it still must be called to set up the command line.
  static void Init(int argc, const char* const* argv);

  // Destroys the current process CommandLine singleton. This is necessary if
  // you want to reset the base library to its initial state (for example in an
  // outer library that needs to be able to terminate, and be re-initialized).
  // If Init is called only once, e.g. in main(), calling Terminate() is not
  // necessary.
  static void Terminate();

  // Get the singleton CommandLine representing the current process's
  // command line.
  static const CommandLine* ForCurrentProcess() {
    DCHECK(current_process_commandline_);
    return current_process_commandline_;
  }

  static bool IsInitialized() {
    return !!current_process_commandline_;
  }

  // Returns true if this command line contains the given switch.
  // (Switch names are case-insensitive.)
  bool HasSwitch(const std::wstring& switch_string) const;

  // Returns the value associated with the given switch.  If the
  // switch has no value or isn't present, this method returns
  // the empty string.
  std::wstring GetSwitchValue(const std::wstring& switch_string) const;

  // Get the remaining arguments to the command.
  // WARNING: this is incorrect on POSIX; we must do string conversions.
  std::vector<std::wstring> GetLooseValues() const;

#if defined(OS_WIN)
  // Returns the original command line string.
  const std::wstring& command_line_string() const {
    return command_line_string_;
  }
#elif defined(OS_POSIX)
  // Returns the original command line string as a vector of strings.
  const std::vector<std::string>& argv() const {
    return argv_;
  }
#endif

  // Returns the program part of the command line string (the first item).
  std::wstring program() const;

  // Return a copy of the string prefixed with a switch prefix.
  // Used internally.
  static std::wstring PrefixedSwitchString(const std::wstring& switch_string);

  // Return a copy of the string prefixed with a switch prefix,
  // and appended with the given value. Used internally.
  static std::wstring PrefixedSwitchStringWithValue(
                        const std::wstring& switch_string,
                        const std::wstring& value_string);

  // Appends the given switch string (preceded by a space and a switch
  // prefix) to the given string.
  void AppendSwitch(const std::wstring& switch_string);

  // Appends the given switch string (preceded by a space and a switch
  // prefix) to the given string, with the given value attached.
  void AppendSwitchWithValue(const std::wstring& switch_string,
                             const std::wstring& value_string);

  // Append a loose value to the command line.
  void AppendLooseValue(const std::wstring& value);

#if defined(OS_WIN)
  void AppendLooseValue(const wchar_t* value) {
    AppendLooseValue(std::wstring(value));
  }
#endif

  // Append the arguments from another command line to this one.
  // If |include_program| is true, include |other|'s program as well.
  void AppendArguments(const CommandLine& other,
                       bool include_program);

  // On POSIX systems it's common to run processes via a wrapper (like
  // "valgrind" or "gdb --args").
  void PrependWrapper(const std::wstring& wrapper);

 private:
  friend class InProcessBrowserTest;

  CommandLine() {}

  // Used by InProcessBrowserTest.
  static CommandLine* ForCurrentProcessMutable() {
    DCHECK(current_process_commandline_);
    return current_process_commandline_;
  }

  // The singleton CommandLine instance representing the current process's
  // command line.
  static CommandLine* current_process_commandline_;

  // We store a platform-native version of the command line, used when building
  // up a new command line to be executed.  This ifdef delimits that code.

#if defined(OS_WIN)
  // The quoted, space-separated command-line string.
  std::wstring command_line_string_;

  // The name of the program.
  std::wstring program_;

  // The type of native command line arguments.
  typedef std::wstring StringType;

#elif defined(OS_POSIX)
  // The argv array, with the program name in argv_[0].
  std::vector<std::string> argv_;

  // The type of native command line arguments.
  typedef std::string StringType;

  // Shared by the two POSIX constructor forms.  Initalize from argv_.
  void InitFromArgv();
#endif

  // Returns true and fills in |switch_string| and |switch_value|
  // if |parameter_string| represents a switch.
  static bool IsSwitch(const StringType& parameter_string,
                       std::string* switch_string,
                       StringType* switch_value);

  // Parsed-out values.
  std::map<std::string, StringType> switches_;

  // Non-switch command-line arguments.
  std::vector<StringType> loose_values_;

  // We allow copy constructors, because a common pattern is to grab a
  // copy of the current process's command line and then add some
  // flags to it.  E.g.:
  //   CommandLine cl(*CommandLine::ForCurrentProcess());
  //   cl.AppendSwitch(...);
};

#endif  // BASE_COMMAND_LINE_H_
