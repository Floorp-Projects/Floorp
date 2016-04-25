// Function template for the following functions:
//   * RegExpLocalReplaceOpt
//   * RegExpLocalReplaceOptFunc
//   * RegExpLocalReplaceOptSubst
// Define the following macro and include this file to declare function:
//   * FUNC_NAME     -- function name (required)
//       e.g.
//         #define FUNC_NAME RegExpLocalReplaceOpt
// Define the following macro (without value) to switch the code:
//   * SUBSTITUTION     -- replaceValue is a string with "$"
//   * FUNCTIONAL       -- replaceValue is a function
//   * neither of above -- replaceValue is a string without "$"

// ES 2017 draft 03bfda119d060aca4099d2b77cf43f6d4f11cfa2 21.2.5.8
// steps 11.a-16.
// Optimized path for @@replace with the following conditions:
//   * global flag is false
function FUNC_NAME(rx, S, lengthS, replaceValue
#ifdef SUBSTITUTION
                   , firstDollarIndex
#endif
                  )
{
    var flags = UnsafeGetInt32FromReservedSlot(rx, REGEXP_FLAGS_SLOT);
    var sticky = !!(flags & REGEXP_STICKY_FLAG);

    var lastIndex;
    if (sticky) {
        lastIndex = ToLength(rx.lastIndex);
        if (lastIndex > lengthS) {
            rx.lastIndex = 0;
            return S;
        }
    } else {
        lastIndex = 0;
    }

    // Step 11.a.
    var result = RegExpMatcher(rx, S, lastIndex);

    // Step 11.b.
    if (result === null) {
        rx.lastIndex = 0;
        return S;
    }

    // Steps 11.c, 12-13, 14.a-b (skipped).

#if defined(FUNCTIONAL) || defined(SUBSTITUTION)
    // Steps 14.a-b.
    var nCaptures = std_Math_max(result.length - 1, 0);
#endif

    // Step 14.c.
    var matched = result[0];

    // Step 14.d.
    var matchLength = matched.length;

    // Step 14.e-f.
    var position = result.index;

    // Step 14.l.iii (reordered)
    // To set rx.lastIndex before RegExpGetComplexReplacement.
    var nextSourcePosition = position + matchLength;

    if (sticky)
       rx.lastIndex = nextSourcePosition;

    var replacement;
    // Steps g-j.
#if defined(FUNCTIONAL)
    replacement = RegExpGetComplexReplacement(result, matched, S, position,

                                              nCaptures, replaceValue,
                                              true, -1);
#elif defined(SUBSTITUTION)
    replacement = RegExpGetComplexReplacement(result, matched, S, position,

                                              nCaptures, replaceValue,
                                              false, firstDollarIndex);
#else
    replacement = replaceValue;
#endif

    // Step 14.l.ii.
    var accumulatedResult = Substring(S, 0, position) + replacement;

    // Step 15.
    if (nextSourcePosition >= lengthS)
        return accumulatedResult;

    // Step 16.
    return accumulatedResult + Substring(S, nextSourcePosition, lengthS - nextSourcePosition);
}
