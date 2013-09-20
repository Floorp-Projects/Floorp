========
mozbuild
========

mozbuild is a Python package providing functionality used by Mozilla's
build system.

Modules Overview
================

* mozbuild.backend -- Functionality for producing and interacting with build
  backends. A build backend is an entity that consumes build system metadata
  (from mozbuild.frontend) and does something useful with it (typically writing
  out files that can be used by a build tool to build the tree).
* mozbuild.compilation -- Functionality related to compiling. This
  includes managing compiler warnings.
* mozbuild.frontend -- Functionality for reading build frontend files
  (what defines the build system) and converting them to data structures
  which are fed into build backends to produce backend configurations.
* mozpack -- Functionality related to packaging builds.

Overview
========

The build system consists of frontend files that define what to do. They
say things like "compile X" "copy Y."

The mozbuild.frontend package contains code for reading these frontend
files and converting them to static data structures. The set of produced
static data structures for the tree constitute the current build
configuration.

There exist entities called build backends. From a high level, build
backends consume the build configuration and do something with it. They
typically produce tool-specific files such as make files which can be used
to build the tree.

Piecing it all together, we have frontend files that are parsed into data
structures. These data structures are fed into a build backend. The output
from build backends is used by builders to build the tree.

