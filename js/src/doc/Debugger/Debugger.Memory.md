# `Debugger.Memory`

If `dbg` is a `Debugger` instance, then `dbg.memory` is an instance of
`Debugger.Memory` whose methods and accessors operate on `dbg`. This class
exists only to hold member functions and accessors related to memory analysis,
keeping them separate from other `Debugger` facilities.

## Accessor Properties of the `Debugger.Memory.prototype` Object

<code id="trackingallocationsites">trackingAllocationSites</code>
:   A boolean value indicating whether this `Debugger.Memory` instance is
    capturing the JavaScript execution stack when each Object is allocated. This
    accessor property has both a getter and setter: assigning to it enables or
    disables the allocation site tracking. Reading the accessor produces `true`
    if the Debugger is capturing stacks for Object allocations, and `false`
    otherwise. Allocation site tracking is initially disabled in a new Debugger.

    Assignment is fallible: if the Debugger cannot track allocation sites, it
    throws an `Error` instance.

    You can retrieve the allocation site for a given object with the
    [`Debugger.Object.prototype.allocationSite`][allocation-site] accessor
    property.
