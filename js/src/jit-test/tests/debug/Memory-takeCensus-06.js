// Check Debugger.Memory.prototype.takeCensus handling of 'breakdown' argument.

load(libdir + 'match.js');
var Pattern = Match.Pattern;

var g = newGlobal();
var dbg = new Debugger(g);

Pattern({ count: Pattern.NATURAL,
          bytes: Pattern.NATURAL })
  .assert(dbg.memory.takeCensus({ breakdown: { by: 'count' } }));

let census = dbg.memory.takeCensus({ breakdown: { by: 'count', count: false, bytes: false } });
assertEq('count' in census, false);
assertEq('bytes' in census, false);

census = dbg.memory.takeCensus({ breakdown: { by: 'count', count: true,  bytes: false } });
assertEq('count' in census, true);
assertEq('bytes' in census, false);

census = dbg.memory.takeCensus({ breakdown: { by: 'count', count: false, bytes: true } });
assertEq('count' in census, false);
assertEq('bytes' in census, true);

census = dbg.memory.takeCensus({ breakdown: { by: 'count', count: true,  bytes: true } });
assertEq('count' in census, true);
assertEq('bytes' in census, true);


// Pattern doesn't mind objects with extra properties, so we'll restrict this
// list to the object classes we're pretty sure are going to stick around for
// the forseeable future.
Pattern({
          Function:       { count: Pattern.NATURAL },
          Object:         { count: Pattern.NATURAL },
          Debugger:       { count: Pattern.NATURAL },
          global:         { count: Pattern.NATURAL },

          // The below are all Debugger prototype objects.
          Source:         { count: Pattern.NATURAL },
          Environment:    { count: Pattern.NATURAL },
          Script:         { count: Pattern.NATURAL },
          Memory:         { count: Pattern.NATURAL },
          Frame:          { count: Pattern.NATURAL }
        })
  .assert(dbg.memory.takeCensus({ breakdown: { by: 'objectClass' } }));

Pattern({
          objects:        { count: Pattern.NATURAL },
          scripts:        { count: Pattern.NATURAL },
          strings:        { count: Pattern.NATURAL },
          other:          { count: Pattern.NATURAL }
        })
  .assert(dbg.memory.takeCensus({ breakdown: { by: 'coarseType' } }));

// As for { by: 'objectClass' }, restrict our pattern to the types
// we predict will stick around for a long time.
Pattern({
          JSString:             { count: Pattern.NATURAL },
          'js::Shape':          { count: Pattern.NATURAL },
          JSObject:             { count: Pattern.NATURAL },
          JSScript:             { count: Pattern.NATURAL }
        })
  .assert(dbg.memory.takeCensus({ breakdown: { by: 'internalType' } }));


// Nested breakdowns.

let coarse_type_pattern = {
  objects:        { count: Pattern.NATURAL },
  scripts:        { count: Pattern.NATURAL },
  strings:        { count: Pattern.NATURAL },
  other:          { count: Pattern.NATURAL }
};

Pattern({
          JSString:    coarse_type_pattern,
          'js::Shape': coarse_type_pattern,
          JSObject:    coarse_type_pattern,
          JSScript:    coarse_type_pattern,
        })
  .assert(dbg.memory.takeCensus({
    breakdown: { by: 'internalType',
                 then: { by: 'coarseType' }
    }
  }));

Pattern({
          Function:       { count: Pattern.NATURAL },
          Object:         { count: Pattern.NATURAL },
          Debugger:       { count: Pattern.NATURAL },
          global:         { count: Pattern.NATURAL },
          other:          coarse_type_pattern
        })
  .assert(dbg.memory.takeCensus({
    breakdown: {
      by: 'objectClass',
      then:  { by: 'count' },
      other: { by: 'coarseType' }
    }
  }));

Pattern({
          objects: { count: Pattern.NATURAL, label: "object" },
          scripts: { count: Pattern.NATURAL, label: "scripts" },
          strings: { count: Pattern.NATURAL, label: "strings" },
          other:   { count: Pattern.NATURAL, label: "other" }
        })
  .assert(dbg.memory.takeCensus({
    breakdown: {
      by: 'coarseType',
      objects: { by: 'count', label: 'object' },
      scripts: { by: 'count', label: 'scripts' },
      strings: { by: 'count', label: 'strings' },
      other:   { by: 'count', label: 'other' }
    }
  }));
