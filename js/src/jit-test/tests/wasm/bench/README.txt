These programs come from the embenchen benchmark suite.  The source
code and Makefiles from which they were built can be found in
https://github.com/lars-t-hansen/embenchen/, in the src directory.

Invariably, the Makefiles were set up with -DJITTEST (see comments in
those files), to configure the programs with very small input problems
that will run to completion in many configurations even on slower
systems.

(Should that repo not be available, it should be possible to piece
together sources from https://github.com/kripken/embenchen and
https://github.com/kripken/emscripten/.)
