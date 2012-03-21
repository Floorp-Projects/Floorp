function f() {
    newGlobal('new-compartment').eval('\
        try {\
          if (typeof gczeal === "function") \
              gczeal(2,1); \
          throw new Error();\
        } catch (e) { \
          gc(); \
          assertEq("" + e, "Error"); \
        } \
    ');
}
f({}, [1,2,4,5,6,7,8,1], new RegExp(), function() {});
