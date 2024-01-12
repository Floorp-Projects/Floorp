# sixgill CFG format

The main output of the sixgill plugin is what is loosely labeled a control flow graph (CFG) associated with each function compiled.
These are stored in the file src_body.xdb, which contains a mapping from function names ("mangled\$unmangled") to function data.

The graph is really a set of directed acyclic data flow graphs, stitched together via "loops" that imply back edges in the control flow graph.

Function data is an array of "bodies", one body for the toplevel code in the function, and another body for each loop. A body is _not_ a basic block, since they can contain interior branches. (The nodes in a body do not necessarily dominate the following nodes.) A body is a DAG, and thus has no back edges or cross edges. Flow starts only at the entry point and ends only at the exit point, though (1) a loop body's entry point implicitly follows its exit point and (2) `Call` nodes will cause the actual program counter to go to another (possibly recursive) body. A body really describes data flow, not dynamic control flow.

## Function Body

A body (whether toplevel or loop) contains:

- .BlockId
  - `.Kind`: "Function" for the toplevel function or "Loop" for a (possibly nested) loop within it.
  - `.Loop`: if .Kind == "Loop", then a string identifier distinguishing the loop, in the format "loop#n" where n is the index of the loop in the body. Nested loops will extend this to "loop#n#m".
  - `.Variable`:
    - `.Kind`: "Func"
    - `.Name[]`: the function `Name` (see below)
- `.Version`: always zero
- `.Command`: the command used to compile this function, if recorded. This command will _not_ include the -fplugin parameters.
- `.Location[]`: a length-2 array of the source positions of the first and last line of the function definition. Hopefully it will be in the same file. Note that this Location is different from a `PPoint.Location` (see below), which will have a single source position. Each source position is:
  - `.CacheString`: the filename
  - `.Line`: the line number
- `.DefineVariable[]`: a list of variables defined in the body. The first one is for the function itself. Each variable has:
  - `.Type`: the type of the variable. See `Type`, below.
  - `.Variable`:
    - `.Kind`: one of
      - "Func" for the function itself
      - "This" for the C++ `this` parameter
      - "Arg" for parameters
      - "Temp" for temporaries
    - `.Name[]`: the variable `Name` (see below)
- `.Index[]`: a 2-tuple of the first and last index in the body.
- `.PPoint[]`: the filename and line number of each point in the body
  - `.Location`: a single source point (see above).
- `.PEdge[]`: the bulk of the body. See Edges, below.
- `.LoopIsomorphic[]`: a list of `{"Index": point}` points in the body that are cloned in loop bodies. See the edge Kind `Loop`, below.

A loop body (a body with BlockId.Kind == "Loop") will additionally have:

- `.BlockPPoint`: an array of full references to points within parent bodies that represent the entry point of this loop. Each has:
  - `.BlockId`: the BlockId of the parent body
  - `.Index`: the index of the point within the parent body
  - `.Version`: the value zero, intended for incremental analyses but unused in the GC hazard analysis.

Note that a loop may appear in more than one parent body. I believe this will not be used for regular structured code, but could be necessary to properly disentangle loops when using `goto`.

`Name`: a 2-tuple containing a variable or function name. The first element is a raw, internal name, and the second is a more user-facing name. For non-functions, both elements are normally the same, but `.Name[0]` could have a `:<n>` suffix if there are multiple variables of that name in different scopes within the same function, or a `<file>:` prefix for static variables. For functions, `.Name[0]` is the full name of the function (in format "mangled\$unmangled") and .Name[1] is the base name of the function (unqualified, with no type or parameters):

    "Variable": {
      "Kind": "Func",
      "Name": [
        "_Z12refptr_test9v$Cell* refptr_test9()",
        "refptr_test9"
      ]
    }

Bodies are an array of "edges" between "points". All behavior is described as happening on these edges. `body.Index[0]` gives the first point in the body. Each edge has a source and destination point. So eg if `body.Index[0]` is 1, then (unless the body is empty) there will be at least one edge with `edge.Index = [1, 2]`. The code `if (C) { x = 1; } else { x = 2; }; f();`, will have two edges sharing a common destination:

    Assume(1,2, C*, true)
    Assign(2,4, x := 1)
    Assume(1,3, C*, false)
    Assign(3,4, x := 2)
    Call(4,5, f())

Note that the above syntax is part of the default output of `xdbfind src_body.xdb <functionName>`. It is a much-simplified version of the full JSON output from `xdbfind -json src_body.xdb <functionName>`. It will be used in this document to describe examples because the JSON output is much too verbose.

Every body is a directed acyclic graph (DAG), stored as a set of edges with source,destination point tuples. Any cycles in the original flow graph are replaced with Loop edges (see below).

## Edges

The edges are stored in an array named `PEdge`, with properties:

- `.Index[]`: a 2-tuple giving the source and destination points.
- `.Kind`: One of 7 different Kinds. The rest of the attributes will depend on this Kind.

Sixgill boils the control flow graph down to a small set of edge Kinds:

### Assign

- `.Exp[]`: a 2-tuple of [lhs, rhs] of the assignment, each an expression (see `Expressions`, below.)
- `.Type`: the overall type of the expression, which I believe is the type of the lhs? (See `Types`, below.)

Note that `Call` is also used for assignments, when the result of the function call is being assigned to a variable.

### Call

- `.Exp[0]`: an expression representing the function being called (the "callee"). The callee might be a simple function, in which case `exp.Kind == "Var"`. Or it could be a computed function pointer or whatever. The expression evaluates to the function being called.
- `.Exp[1]` (optional): where to assign the return value.
- `.PEdgeCallArguments[]`: an array of expressions, one for each argument being passed. This does not include the `this` argument.
- `.PEdgeCallInstance`: the expression for the object to call the method on, which will be passed as the `this` argument.

### Assume

The destination of an `Assume` node can rely on the given value assumption, eg `Assume(1,2, __temp_1* == 7)` means that `__temp_1` will be 7 at point 2.

A conditional branch will be represented as a pair of `Assume` edges coming off of the expression for the branch condition. These edges produce a data flow graph where you can know the value of a variable if it has passed through an `Assume` edge (at least, until it reaches an `Assign` or `Call` edge.)

- `.Exp`: the expression being tested.
- `.PEdgeAssumeNonZero`: if present, this will be set to true, and means we are on the edge where `Exp` is `!= 0`. If this is not present, then `Exp` is `0`.

Example: the C++ function body

    SomeRAIIType raii;
    if (flipcoin()) {
      return 1;
    } else {
      return 2;
    }

could produce something like:

    Call(3,4, __temp_1 := flipcoin())
    Assume(4,5, __temp_1*, true)
    Assume(4,6, __temp_1*, false)
    Assign(5,7, return := 1)
    Assign(6,7, return := 2)
    Call(7,8, raii.~__dt_comp ())

### Loop

The edge corresponds to an entire loop. The meaning of a "loop" is subtle. It is mainly what is required to convert a general graph into a set of acyclic DAGs by finding back edges, and creating a "loop body" from the subgraph between the entry point (the destination of the back edge) and the source of the back edge. (Multiple back edges with a common destination will be a single loop.) Only the main body nodes that are necessary for (postdominated by) one of the back edges will be removed. Shared nodes will be cloned and will appear in both the main body and the loop body. The cloned nodes are described as "isomorphic".

- `.BlockId` : the `BlockId` of the loop body.
- `.Loop` : an id like "loop#0" that will match up with the .BlockId.Loop property of the corresponding loop body.

Example: consider the C++ code

    float testfunc(int val) {
      int x = val;
      x++;
    loophead:
      int y = x + 2;
      if (y == 8) goto loophead;
      y++;
      if (y == 10) return 2.4;
      if (y == 12) goto loophead;
      return 3.6;
    }

This will produce the loop body:

    block: float32 testfunc(int32):loop#0
    parent: float32 testfunc(int32):3
    pentry: 1
    pexit:  6
    Assign(1,2, y := (x* + 2))
    Assume(2,6, (y* == 8), true)  /* 6 is the exit point, so loops back to the entry point 1 */
    Assume(2,3, (y* == 8), false)
    Assign(3,4, y := (y* + 1))
    Assume(4,5, (y* == 10), false)
    Assume(5,6, (y* == 12), true) /* 6 is the exit point, so loops back to the entry point 1 */

and the main body:

    block: float32 testfunc(int32)
    pentry: 1
    pexit:  11
    isomorphic: [4,5,6,7,9]
    Assign(1,2, x := val*)
    Assign(2,3, x := (x* + 1))
    Loop(3,4, loop#0)
    Assign(4,5, y := (x* + 2))       /* edge is also in the loop */
    Assume(5,6, (y* == 8), false)    /* edge is also in the loop */
    Assign(6,7, y := (y* + 1))       /* edge is also in the loop */
    Assume(7,8, (y* == 10), true)
    Assume(7,9, (y* == 10), false)   /* edge is also in the loop */
    Assign(8,11, return := 2.4)
    Assume(9,10, (y* == 12), false)
    Assign(10,11, return := 3.6)

The isomorphic points correspond to the C++ code:

    y = x + 2;
    if (y == 8) /* when y != 8 */
    y++;
    if (y == 10) /* when y != 10 */

which is the code that will execute in order to reach the post-loop edge `Assume(9,10, (y* == 12), false)`. (If point 9 in the main body is reached and y _is_ equal to 12, then the `Assume(9,10,...)` edge will not be taken. Point 9 in the main body corresponds to point 5 in the loop body, so the edge `Assume(5,6, (y* == 12), true)` will be taken instead.) When "control flow" is at an isomorphic point, it can be considered to be at all "instantiations" of that point at the same time. Really, though, these are acyclic data flow graphs where a loop's exit point is externally known to flow into the entry point, and the main body lacks any `Assume` or other back edges that would make it cyclic.

For a `while` loop, the isomorphic points will evaluate the conditional expression.

Another example: the C++ code

    void testfunc() {
      static Cell cell;
      RefPtr<float> v10;
      v10.assign_with_AddRef(&somefloat);
      while (flipcoin()) {
        v10.forget();
      }
    }

generates

    block: void testfunc():loop#0
    parent: void testfunc():3
    pentry: 1
    pexit:  4
    Call(1,2, __temp_1 := flipcoin())
    Assume(2,3, __temp_1*, true)
    Call(3,4, v10.forget())

    block: void testfunc()
    pentry: 1
    pexit:  7
    isomorphic: [3,4]
    Call(1,2, v10.assign_with_AddRef(somefloat))
    Loop(2,3, loop#0)
    Call(3,4, __temp_1 := flipcoin())
    Assume(4,5, __temp_1*, false)
    Call(5,6, v10.~__dt_comp ())

The first block is the loop body, the second is the main body. Points 3 and 4 of the main body are equivalent to points 1 and 2 of the loop body. Notice the "parent" field of the loop body, which gives the equivalent point (3) of the loop's entry point in the body main.

### Assembly

An opaque wad of assembly code.

### Annotation

I'm not sure if I've seen these? They might be for the old annotation mechanism.

### Skip

These appear to be internal "epsilon" edges to simplify graph building and loop splitting. They are removed before the final CFG is emitted.

## Expressions

Expressions are the bulk of the CFG.

- `.Width` (optional) : width in bits. I'm not sure when this is used. It is much more common for a Type to have a width.
- `.Unsigned` (optional) : boolean saying that this expression is unsigned.
- `.Kind` : one of the following values

### Program lvalues

- "Empty" : used in limited contexts when nothing is needed.
- "Var" : expression referring to a variable
  - `.Type`
- "Drf" : dereference (as in, \*foo or foo->... or something implicit)
  - `.Exp[0]` : target being dereferenced
  - `.Type`
- "Fld"
  - `.Exp[0]` : target object containing the field
  - `.Field`
    - `.Name[]` : 2-tuple of [qualified name, unqualified name]
      - can be unnamed, in which case the name will be "field:<number>". This is used for base classes.
    - `.FieldCSU` : type of the CSU that the field is a member of
    - `.Type` : type of the field
    - `.FieldInstanceFunction` : "whether this is a virtual instance function rather than data field of the containing CSU". Presence or absence is what matters. All examples I have seen are for pure virtual functions (`virtual void foo() = 0`).
    - `.Annotation[]` : any annotations on the specific field
- "Rfld" : ? some kind of "reverse" field access
  - same children as Fld
- "Index" : array element access
  - `.Exp[0]` : the target array
  - `.Index` : the index being accessed (an Exp)
  - `.Type` : the type of the element
- "String" : string constant
  - `.Type` : the type of the string
  - `.Count` : number of elements (chars) in the string
  - `.String` : the actual data in the string
- "Clobber" : "additional lvalue generated by the memory model" (?)
  - callee
  - overwrite
  - optional value kind
  - point
  - optional location

### Program rvalues

- "Int", "Float" : constant values
  - `.String` : the string form of the value (this is the only way the value is stored)
- "Unop", "Binop" : operators
  - `.OpCode` : the various opcodes
  - `.Exp[0]` and `.Exp[1]` (the latter for Binop only) : parameters
  - stride type (optional)

### Expression modifiers

- "Exit", "Initial" : ?
  - `.Exp[0]` : target expression
  - value kind (optional)
- "Val" : ?
  - lvalue
  - value kind (optional)
  - index (body point)
  - boolean saying whether it is relative (?)
- "Frame" : (unused)

### Immutable properties

These appear to be synthetic properties intended for the built-in analyses that we are not using.

- "NullTest" : ?
  - `.Exp[0]` : target being tested
- "Bound" : ? appears to be bounds-checked index access
  - bound kind
  - stride type
  - `.Exp[0]` (optional) : target that the bound applies to
- "Directive" : ?
  - directive kind

### Mutable properties

These appear to be synthetic properties intended for the built-in analyses that we are not using.

- "Terminate"
  - stride type
  - terminate test (Exp)
  - terminate int (Exp)
  - `.Exp[0]` (optional) : target
- "GCSafe" : (unused)
  - `.Exp[0]` (optional) : target

## Types

- `.Kind` : the kind of type being described, one of:

Possible Type Kinds:

- "Void" : the C/C++ void type
- "Int"
  - `.Width` : width in bits
  - `.Sign` (optional) : whether the type is signed
  - `.Variant` (optional) : ?
- "Float"
  - `.Width` : width in bits
- "Pointer" : pointer or reference type
  - `.Width` : width in bits
  - `.Reference` : 0 for pointer, 1 for regular reference, 2 for rvalue reference
  - `.Type` : type of the target
- "Array"
  - `.Type` : type of the elements
  - `.Count` : number of elements, given as a plain constant integer
- "CSU" : class, structured, or union
  - `.Name` : qualified name, as a plain string
- "Function"
  - `.TypeFunctionCSU` (optional) : if present, the type of the CSU containing the function
  - `.FunctionVarArgs` (?) (optional) : if this is present, the function is varargs (eg f(...))
  - `.TypeFunctionArgument` : array of argument types. Present if at least one parameter.
    - `.Type` : type of argument
    - `.Annotation` (optional) : any explicit annotations (**attribute**((foo))) for this parameter
  - `.Variable` : the variable representing the function
  - `.Annotation` (optional) : any explicit annotation for this function
- "Error" : there was an error handling this type in sixgill. Probably something unimplemented.
