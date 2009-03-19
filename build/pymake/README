INTRODUCTION

make.py (and the pymake modules that support it) are an implementation of the make tool
which are mostly compatible with makefiles written for GNU make.

PURPOSE

The Mozilla project inspired this tool with several goals:

* Improve build speeds, especially on Windows. This can be done by reducing the total number
  of processes that are launched, especially MSYS shell processes which are expensive.

* Allow writing some complicated build logic directly in Python instead of in shell.

* Allow computing dependencies for special targets, such as members within ZIP files.

* Enable experiments with build system. By writing a makefile parser, we can experiment
  with converting in-tree makefiles to another build system, such as SCons, waf, ant, ...insert
  your favorite build tool here. Or we could experiment along the lines of makepp, keeping
  our existing makefiles, but change the engine to build a global dependency graph.

KNOWN INCOMPATIBILITIES

* Order-only prerequisites are not yet supported

* Secondary expansion is not yet supported.

* Target-specific variables behave differently than in GNU make: in pymake, the target-specific
  variable only applies to the specific target that is mentioned, and does not apply recursively
  to all dependencies which are remade. This is an intentional change: the behavior of GNU make
  is neither deterministic nor intuitive.

* $(eval) is only supported during the parse phase. Any attempt to recursively expand
  an $(eval) function during command execution will fail. This is an intentional incompatibility.

* There is a subtle difference in execution order that can cause unexpected changes in the
  following circumstance:
** A file `foo.c` exists on the VPATH
** A rule for `foo.c` exists with a dependency on `tool` and no commands
** `tool` is remade for some other reason earlier in the file
  In this case, pymake resets the VPATH of `foo.c`, while GNU make does not. This shouldn't
  happen in the real world, since a target found on the VPATH without commands is silly. But
  mozilla/js/src happens to have a rule, which I'm patching.

* pymake does not implement any of the builtin implicit rules or the related variables. Mozilla
  only cares because pymake doesn't implicitly define $(RM), which I'm also fixing in the Mozilla
  code.

ISSUES

* Speed is a problem.

FUTURE WORK

* implement a new type of command which is implemented in python. This would allow us
to replace the current `nsinstall` binary (and execution costs for the shell and binary) with an
in-process python solution.

AUTHOR

Initial code was written by Benjamin Smedberg <benjamin@smedbergs.us>. For future releases see
http://benjamin.smedbergs.us/pymake/

See the LICENSE file for license information (MIT license)
