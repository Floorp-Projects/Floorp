# So You Want to Add a Bytecode Op

Occasionally we need to add a new bytecode operation in order to express or optimize
some feature of JavaScript. This document is intended to assist you in determining if
you ought to add a bytecode op, and what kind of changes and integrations will be
required if you do this.

Think of this as a more specialized version of the feature addition document.

## First: Do you need a new bytecode op?

There are alternatives to increasing the bytecode space!

* [Self-hosted intrinsics][intrinsic] can be accessed directly from the bytecode
  generator using the `GetIntrinsic` bytecode. For an example see
  [`BytecodeEmitter::emitCopyDataProperties`][emitCopy]. Calls to intrinsics can be
  [(optionally) optimized with CacheIR as well][optimize].
* Desugar the behavior you want into a sequence of already existing ops. If this is
  possible this is often the right choice: Support for everything comes along mostly
  for free.
  * Is it possible to teach the JIT compilers to recognize your sequence of bytecode
    if special handling is required? If that kind of idiom recognition is too costly,
    you may be better served by a new bytecode op.


## Second: Can we make your bytecode fast?

To implement bytecode in our JIT compilers we put a few constraints on our bytecode.
e.g. Each bytecode must be atomic, to support bailouts and exception behaviour. This
means that your op must not create torn objects or invalid states.

Ideally there would be a fast inline cache possible to implement your bytecode, as
this brings speed and performance to our BaselineInterpreter layer, rather than
waiting for Warp.

ICs are valuable when behaviour varies substantially at runtime (ie, we choose
different paths based on operand types). Examples of where we've used this are
`JSOp::OptimizeSpreadCall` and `JSOp::CloseIter`, both of which let us generate a
fastpath in the common case or fall back to something more expensive if necessary.


[intrinsic]: https://searchfox.org/mozilla-central/search?q=intrinsic_&path=js%2Fsrc%2F&case=false&regexp=false
[emitCopy]: https://searchfox.org/mozilla-central/rev/650c19c96529eb28d081062c1ca274bc50ef3635/js/src/frontend/BytecodeEmitter.cpp#5018,5027,5039,5045,5050,5055,5067
[optimize]: https://searchfox.org/mozilla-central/rev/c1180ea13e73eb985a49b15c0d90e977a1aa919c/js/src/jit/CacheIR.cpp#10140
