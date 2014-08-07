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

`maxAllocationsLogLength`
:   The maximum number of allocation sites to accumulate in the allocations log
    at a time. Its default value is `5000`.

## Function Properties of the `Debugger.Memory.prototype` Object

`drainAllocationsLog()`
:   When `trackingAllocationSites` is `true`, this method returns an array of
    allocation sites (as [captured stacks][saved-frame]) for recent `Object`
    allocations within the set of debuggees. *Recent* is defined as the
    `maxAllocationsLogLength` most recent `Object` allocations since the last
    call to `drainAllocationsLog`. Therefore, calling this method effectively
    clears the log.

    When `trackingAllocationSites` is `false`, `drainAllocationsLog()` throws an
    `Error`.
