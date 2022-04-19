// Specifying an owning element in a cross-global evaluation shouldn't crash.
// That is, when 'evaluate' switches compartments, it should properly wrap
// the CompileOptions members that will become cross-compartment
// references.

evaluate('42 + 1729', { global: newGlobal(), element: {} });
