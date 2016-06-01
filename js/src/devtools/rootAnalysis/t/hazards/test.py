test.compile("source.cpp")
test.run_analysis_script('gcTypes')

# gcFunctions should be the inverse, but we get to rely on unmangled names here.
gcFunctions = test.load_gcFunctions()
print(gcFunctions)
assert('void GC()' in gcFunctions)
assert('void suppressedFunction()' not in gcFunctions)
assert('void halfSuppressedFunction()' in gcFunctions)
assert('void unsuppressedFunction()' in gcFunctions)
assert('Cell* f()' in gcFunctions)

hazards = test.load_hazards()
hazmap = {haz.variable: haz for haz in hazards}
assert('cell1' not in hazmap)
assert('cell2' in hazmap)
assert('cell3' in hazmap)
assert('cell4' not in hazmap)
assert('cell5' not in hazmap)
assert('cell6' not in hazmap)
assert('<returnvalue>' in hazmap)

# All hazards should be in f()
assert(hazmap['cell2'].function == 'Cell* f()')
assert(len(set(haz.function for haz in hazards)) == 1)

# Check that the correct GC call is reported for each hazard. (cell3 has a
# hazard from two different GC calls; it doesn't really matter which is
# reported.)
assert(hazmap['cell2'].GCFunction == 'void halfSuppressedFunction()')
assert(hazmap['cell3'].GCFunction in ('void halfSuppressedFunction()', 'void unsuppressedFunction()'))
assert(hazmap['<returnvalue>'].GCFunction == 'void GCInDestructor::~GCInDestructor()')

# Type names are handy to have in the report.
assert(hazmap['cell2'].type == 'Cell*')
assert(hazmap['<returnvalue>'].type == 'Cell*')
