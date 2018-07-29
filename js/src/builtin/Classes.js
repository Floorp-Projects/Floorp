// Give a builtin constructor that we can use as the default. When we give
// it to our newly made class, we will be sure to set it up with the correct name
// and .prototype, so that everything works properly.

// The template classes should have all their code on a single line, so that
// bytecodes' line offsets within the script don't contribute spurious offsets
// when we transplant them into the real class definition. Punting on column numbers.

var DefaultDerivedClassConstructor =
    class extends null { constructor(...args) { super(...allowContentIter(args)); } };
MakeDefaultConstructor(DefaultDerivedClassConstructor);

var DefaultBaseClassConstructor =
    class { constructor() { } };
MakeDefaultConstructor(DefaultBaseClassConstructor);
