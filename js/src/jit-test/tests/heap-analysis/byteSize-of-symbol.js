// Check JS::ubi::Node::size results for symbols.

// We actually hard-code specific sizes into this test, even though they're
// implementation details, because in practice there are only two architecture
// variants to consider (32-bit and 64-bit), and if these sizes change, that's
// something SpiderMonkey hackers really want to know; they're supposed to be
// stable.

// Run this test only if we're using jemalloc. Other malloc implementations
// exhibit surprising behaviors. For example, 32-bit Fedora builds have
// non-deterministic allocation sizes.
var config = getBuildConfiguration();
if (!config['moz-memory'])
  quit(0);

const SIZE_OF_SYMBOL = config['pointer-byte-size'] == 4 ? 16 : 24;

// Without a description.
assertEq(byteSize(Symbol()), SIZE_OF_SYMBOL);

// With a description.
assertEq(byteSize(Symbol("This is a relatively long description to be passed to "
                         + "Symbol() but it doesn't matter because it just gets "
                         + "interned as a JSAtom* anyways.")),
         SIZE_OF_SYMBOL);
