# 'test' is provided by the calling script.
# flake8: noqa: F821

test.compile("source.cpp")
test.run_analysis_script('gcTypes')

# The suppressions file uses only mangled names since it's for internal use,
# though I may change that soon given (1) the unfortunate non-uniqueness of
# mangled constructor names, and (2) the usefulness of this file for
# mrgiggles's reporting.
suppressed = test.load_suppressed_functions()

# gcFunctions should be the inverse, but we get to rely on unmangled names here.
gcFunctions = test.load_gcFunctions()

assert 'void Sub1::noneGC()' not in gcFunctions
assert 'void Sub1::someGC()' not in gcFunctions
assert 'void Sub1::allGC()' in gcFunctions
assert 'void Sub2::noneGC()' not in gcFunctions
assert 'void Sub2::someGC()' in gcFunctions
assert 'void Sub2::allGC()' in gcFunctions

callgraph = test.load_callgraph()
assert callgraph.calleeGraph['void f()']['Super.noneGC']
assert callgraph.calleeGraph['Super.noneGC']['void Sub1::noneGC()']
assert callgraph.calleeGraph['Super.noneGC']['void Sub2::noneGC()']
assert 'void Sibling::noneGC()' not in callgraph.calleeGraph['Super.noneGC']

hazards = test.load_hazards()
hazmap = {haz.variable: haz for haz in hazards}
assert 'c1' not in hazmap
assert 'c2' in hazmap
assert 'c3' in hazmap
assert 'c4' not in hazmap
assert 'c5' in hazmap
assert 'c6' in hazmap
assert 'c7' not in hazmap
assert 'c8' in hazmap
assert 'c9' in hazmap
assert 'c10' in hazmap
assert 'c11' in hazmap
