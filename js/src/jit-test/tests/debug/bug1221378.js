// Bug 1221378: allocating an array from within the object metadata callback
// shouldn't cause re-entrant resolution of lazy prototypes.

// To test for this regression, we need some way to make the code that collects
// metadata about object allocations allocate an Array. Presently,
// enableShellObjectMetadataCallback installs a callback that does this, but if
// that hook gets removed (in production there's only ever one callback we
// actually use), then the ability to make whatever metadata collection code
// remains allocate an Array will cover this regression. For example, it could
// be a flag that one can only set in debug builds from TestingFunctions.cpp.
newGlobal().eval('enableShellAllocationMetadataBuilder(); Array');
