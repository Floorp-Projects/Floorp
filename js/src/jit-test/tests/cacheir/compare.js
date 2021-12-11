setJitCompilerOption('ion.forceinlineCaches', 1);

function warmup(fun, input_array) {
    for (var index = 0; index < input_array.length; index++) {
        input = input_array[index];
        input_lhs = input[0];
        input_rhs = input[1];
        output    = input[2];
        for (var i = 0; i < 30; i++) {
            var str = "";
            var y = fun(input_lhs, input_rhs);
            if (y != output) {
                str = "Computed: "+y+ ", expected: "+ output + " ( " + fun + " lhs: "+ input_lhs + " rhs: " + input_rhs +")";
            }
            assertEq(str, "");
        }
    }
}


// Int32 + Int32
var Int32Int32Fun_GT1 = (a, b) => { return a > b; }
warmup(Int32Int32Fun_GT1, [[1,2, false], [1,1, false], [3,4, false], [4294967295, 2, true],
                           [NaN, 2, false], [-1000, NaN, false], [NaN, NaN, false], [10, undefined, false]]);

var Int32Int32Fun_GTE1 = (a, b) => { return a >= b; }
warmup(Int32Int32Fun_GTE1, [[1,2, false], [1,1, true], [3,4, false], [4294967295, 2, true],
                            [NaN, 2, false], [-1000, NaN, false], [NaN, NaN, false], [10, undefined, false]]);

var Int32Int32Fun_LT1 = (a, b) => { return a < b; }
warmup(Int32Int32Fun_LT1, [[1,2, true], [1,1, false], [3,4, true], [4294967295, 2, false],
                           [NaN, 2, false],[-1000, NaN, false], [NaN, NaN, false], [10, undefined, false]]);

var Int32Int32Fun_LTE1 = (a, b) => { return a <= b; }
warmup(Int32Int32Fun_LTE1, [[1,2, true], [1,1, true], [3,4, true], [4294967295, 2, false],
                            [NaN, 2, false], [-1000, NaN, false], [NaN, NaN, false], [10, undefined, false]]);

var Int32Int32Fun_EQ1 = (a, b) => { return a == b; }
warmup(Int32Int32Fun_EQ1, [[1,2, false], [1,1, true], [3,4, false], [4294967295, 2, false],
                            [NaN, 2, false], [-1000, NaN, false], [undefined, null, true],
                            ['0', 0, true], [new String('0'), 0, true], [10, undefined, false]]);

var Int32Int32Fun_EQ2 = (a, b) => { return a == b; }
warmup(Int32Int32Fun_EQ2, [[1, NaN, false], [1, undefined, false], [1, null, false]]);

var Int32Int32Fun_EQ3 = (a, b) => { return a == b; }
warmup(Int32Int32Fun_EQ3, [[{a: 1}, NaN, false], [{a: 1}, undefined, false], [{a: 1}, null, false]]);

var Int32Int32Fun_EQ4 = (a, b) => { return a == b; }
warmup(Int32Int32Fun_EQ4, [[undefined, undefined, true], [null, null, true], [null, undefined, true], [undefined, null, true]]);


var Int32Int32Fun_NEQ1 = (a, b) => { return a != b; }
warmup(Int32Int32Fun_NEQ1, [[1,2, true], [1,1, false], [3,4, true], [4294967295, 2, true],
                            [NaN, 2, true], [-1000, NaN, true], [undefined, null, false],
                            ['0', 0, false], [new String('0'), 0, false], [10, undefined, true]]);

var Int32Int32Fun_NEQ2 = (a, b) => { return a != b; }
warmup(Int32Int32Fun_NEQ2, [[1, NaN, true], [1, undefined, true], [1, null, true]]);

var Int32Int32Fun_NEQ3 = (a, b) => { return a != b; }
warmup(Int32Int32Fun_NEQ3, [[{a: 1}, NaN, true], [{a: 1}, undefined, true], [{a: 1}, null, true]]);

var Int32Int32Fun_NEQ4 = (a, b) => { return a != b; }
warmup(Int32Int32Fun_NEQ4, [[undefined, undefined, false], [null, null, false], [null, undefined, false], [undefined, null, false]]);

var Int32Int32Fun_SEQ1 = (a, b) => { return a === b; }
warmup(Int32Int32Fun_SEQ1, [[1,2, false], [1,1, true], [3,4, false], [4294967295, 2, false],
                            [NaN, 2, false], [-1000, NaN, false], [undefined, null, false],
                            ['0', 0, false], [new String('0'), 0, false], [10, undefined, false]]);

var Int32Int32Fun_SEQ2 = (a, b) => { return a === b; }
warmup(Int32Int32Fun_SEQ2, [[1, NaN, false], [1, undefined, false], [1, null, false]]);

var Int32Int32Fun_SEQ3 = (a, b) => { return a === b; }
warmup(Int32Int32Fun_SEQ3, [[{a: 1}, NaN, false], [{a: 1}, undefined, false], [{a: 1}, null, false]]);

var Int32Int32Fun_SEQ4 = (a, b) => { return a === b; }
warmup(Int32Int32Fun_SEQ4, [[undefined, undefined, true], [null, null, true], [null, undefined, false], [undefined, null, false] ]);

var Int32Int32Fun_SEQ5 = (a, b) => { return a === b; }
warmup(Int32Int32Fun_SEQ5, [[1, true, false], [1, false, false], [false, 1, false], [true, 0, false], [true, true, true]]);

var Int32Int32Fun_SNEQ1 = (a, b) => { return a !== b; }
warmup(Int32Int32Fun_SNEQ1, [[1,2, true], [1,1, false], [3,4, true], [4294967295, 2, true],
                            [NaN, 2, true], [-1000, NaN, true], [undefined, null, true],
                            ['0', 0, true], [new String('0'), 0, true], [10, undefined, true]]);

var Int32Int32Fun_SNEQ2 = (a, b) => { return a !== b; }
warmup(Int32Int32Fun_SNEQ2, [[1, NaN, true], [1, undefined, true], [1, null, true]]);

var Int32Int32Fun_SNEQ3 = (a, b) => { return a !== b; }
warmup(Int32Int32Fun_SNEQ3, [[{a: 1}, NaN, true], [{a: 1}, undefined, true], [{a: 1}, null, true]]);

var Int32Int32Fun_SNEQ4 = (a, b) => { return a !== b; }
warmup(Int32Int32Fun_SNEQ4, [[undefined, undefined, false], [null, null, false], [null, undefined, true], [undefined, null, true] ]);

var Int32Int32Fun_SNEQ5 = (a, b) => { return a !== b; }
warmup(Int32Int32Fun_SNEQ5, [[1, true, true], [1, false, true], [false, 1, true], [true, 0, true]]);


// Number Number
var NumberNumberFun_GT1 = (a, b) => { return a > b; }
warmup(NumberNumberFun_GT1, [[1,2, false], [1.3, 2, false], [1, 2.6, false], [1.2,2.2, false],
                             [1,1, false], [3,4, false], [4294967295, 2, true],
                             [NaN, 2, false], [-1000, NaN, false], [NaN, NaN, false], [10.2, undefined, false]]);

var NumberNumberFun_GTE1 = (a, b) => { return a >= b; }
warmup(NumberNumberFun_GTE1, [[1,2, false], [1.3, 2, false], [1, 2.6, false], [1.2,2.2, false],
                             [1,1, true], [3,4, false], [4294967295, 2, true],
                              [NaN, 2, false], [-1000, NaN, false], [NaN, NaN, false], [10.2, undefined, false]]);

var NumberNumberFun_LT1 = (a, b) => { return a < b; }
warmup(NumberNumberFun_LT1, [[1,2, true], [1.3, 2, true], [1, 2.6, true], [1.2,2.2, true],
                             [1,1, false], [3,4, true], [4294967295, 2, false],
                             [NaN, 2, false],[-1000, NaN, false], [NaN, NaN, false, [10.2, undefined, false]]]);

var NumberNumberFun_LTE1 = (a, b) => { return a <= b; }
warmup(NumberNumberFun_LTE1, [[1,2, true], [1.3, 2, true], [1, 2.6, true], [1.2,2.2, true],
                              [1,1, true], [3,4, true], [4294967295, 2, false],
                              [NaN, 2, false], [-1000, NaN, false], [NaN, NaN, false], [10.2, undefined, false]]);

var NumberNumberFun_EQ1 = (a, b) => { return a == b; }
warmup(NumberNumberFun_EQ1, [[1,2, false], [1.3, 2, false], [1, 2.6, false], [1.2,2.2, false],
                             [1,1, true], [3,4, false], [4294967295, 2, false],
                              [NaN, 2, false], [-1000, NaN, false], [undefined, null, true],
                            ['0', 0, true], [new String('0'), 0, true], [10.2, undefined, false]]);

var NumberNumberFun_NEQ1 = (a, b) => { return a != b; }
warmup(NumberNumberFun_NEQ1, [[1,2, true], [1.3, 2, true], [1, 2.6, true], [1.2,2.2, true],
                              [1,1, false], [3,4, true], [4294967295, 2, true],
                              [NaN, 2, true], [-1000, NaN, true], [undefined, null, false],
                            ['0', 0, false], [new String('0'), 0, false], [10.2, undefined, true]]);

var NumberNumberFun_SEQ1 = (a, b) => { return a === b; }
warmup(NumberNumberFun_SEQ1, [[1,2, false], [1.3, 2, false], [1, 2.6, false], [1.2,2.2, false],
                             [1,1, true], [3,4, false], [4294967295, 2, false],
                              [NaN, 2, false], [-1000, NaN, false], [undefined, null, false],
                            ['0', 0, false], [new String('0'), 0, false], [10.2, undefined, false]]);

var NumberNumberFun_SNEQ1 = (a, b) => { return a !== b; }
warmup(NumberNumberFun_SNEQ1, [[1,2, true], [1.3, 2, true], [1, 2.6, true], [1.2,2.2, true],
                               [1,1, false], [3,4, true], [4294967295, 2, true],
                               [NaN, 2, true], [-1000, NaN, true], [undefined, null, true],
                               ['0', 0, true], [new String('0'), 0, true], [10.2, undefined, true]]);

// Boolean + Int32
var BooleanFun_GT1 = (a, b) => { return a > b; }
warmup(BooleanFun_GT1, [[1,2, false], [true, 2, false], [1,1, false], [true,true, false], [3,4, false], ]);

var BooleanFun_GTE1 = (a, b) => { return a >= b; }
warmup(BooleanFun_GTE1, [[1,2, false], [true, 2, false], [1,1, true], [true,true, true], [3,4, false]]);

var BooleanFun_LT1 = (a, b) => { return a < b; }
warmup(BooleanFun_LT1, [[1,2, true], [true, 2, true], [1,1, false], [true,true, false], [3,4, true]]);

var BooleanFun_LTE1 = (a, b) => { return a <= b; }
warmup(BooleanFun_LTE1, [[1,2, true], [true, 2, true], [1,1, true], [true,true, true], [3,4, true]]);

var BooleanFun_EQ1 = (a, b) => { return a == b; }
warmup(BooleanFun_EQ1, [[1,2, false], [true, 2, false], [1,1, true], [true,true, true], [3,4, false],
               ['0', 0, true], [new String('0'), 0, true], [10, undefined, false]]);

var BooleanFun_NEQ1 = (a, b) => { return a != b; }
warmup(BooleanFun_NEQ1, [[1,2, true], [true, 2, true], [1,1, false], [true,true, false], [3,4, true],
             ['0', 0, false], [new String('0'), 0, false], [10, undefined, true]]);

var BooleanFun_SEQ1 = (a, b) => { return a === b; }
warmup(BooleanFun_SEQ1, [[1,2, false], [true, 2, false], [1,1, true], [true,true, true], [3,4, false]]);

var BooleanFun_SNEQ1 = (a, b) => { return a !== b; }
warmup(BooleanFun_SNEQ1, [[1,2, true], [true, 2, true], [1,1, false], [true,true, false], [3,4, true]]);

// Undefined / Null equality.
var UndefNull_EQ1 = (a, b) => { return a == b; }
warmup(UndefNull_EQ1, [[undefined, null, true], [undefined, undefined, true], [null, undefined, true],
                        [null, null, true], [{a: 10}, undefined, false], [{a: 10}, null, false]]);

var UndefNull_NEQ1 = (a, b) => { return a != b; }
warmup(UndefNull_NEQ1, [[undefined, null, false], [undefined, undefined, false], [null, undefined, false],
                         [null, null, false],  [{a: 10}, undefined, true], [{a: 10}, null, true]]);

var UndefNull_SEQ1 = (a, b) => { return a === b; }
warmup(UndefNull_SEQ1, [[undefined, null, false], [undefined, undefined, true], [null, undefined, false],
                         [null, null, true],  [{a: 10}, undefined, false], [{a: 10}, null, false]]);

var UndefNull_SNEQ1 = (a, b) => { return a !== b; }
warmup(UndefNull_SNEQ1, [[undefined, null, true], [undefined, undefined, false], [null, undefined, true],
                          [null, null, false],  [{a: 10}, undefined, true], [{a: 10}, null, true]]);

var typeDifference = function(a,b) { return a === b; }
warmup(typeDifference, [[1,1, true], [3,3, true], [3, typeDifference, false],[typeDifference, {}, false],
                        [3.2, 1, false], [0, -0, true]]);

// String + Number
var String_Number_GT1 = (a, b) => { return a > b; }
warmup(String_Number_GT1, [["1.4",2, false], [1,"1", false], ["3",4, false],
                           [-1024, "-1023", false], [1.3, "1.2", true]]);

var String_Number_GTE1 = (a, b) => { return a >= b; }
warmup(String_Number_GTE1, [["1.4",2, false], [1,"1", true], [3,"4", false],
                            [-1024, "-1023", false], [1.2, "1.2", true]]);

var String_Number_LT1 = (a, b) => { return a < b; }
warmup(String_Number_LT1, [["1.4",2, true], ["1",1, false], [3,"4", true],
                           [-1024, "-1023", true], [1.2, "1.2", false]]);

var String_Number_LTE1 = (a, b) => { return a <= b; }
warmup(String_Number_LTE1, [["1.4",2, true], ["1",1, true], [3,"4", true],
                            [-1024, "-1023", true], [1.2, "1.2", true]]);

var String_Number_EQ1 = (a, b) => { return a == b; }
warmup(String_Number_EQ1, [["1.4",2, false], ["1",1, true], [3,"4", false],
                          [-1024, "-1023", false], [1.2, "1.2", true]]);

var String_Number_NEQ1 = (a, b) => { return a != b; }
warmup(String_Number_NEQ1, [["1.4",2, true], ["1",1, false], [3,"4", true],
                            [-1024, "-1023", true], [1.2, "1.2", false]]);

var String_Number_SEQ1 = (a, b) => { return a === b; }
warmup(String_Number_SEQ1, [["1.4",2, false],  ["1",1, false], [3,"4", false],
                            [-1024, "-1023", false], [1.2, "1.2", false]]);

var String_Number_SNEQ1 = (a, b) => { return a !== b; }
warmup(String_Number_SNEQ1, [["1.4",2, true], ["1",1, true], [3,"4", true],
                             [-1024, "-1023", true], [1.2, "1.2", true]]);

// String + String
var String_String_GT1 = (a, b) => a > b;
warmup(String_String_GT1, [["", "", false], ["a", "a", false], ["aa", "aa", false],
                           ["a", "", true], ["", "a", false],
                           ["a", "b", false], ["b", "a", true],
                           ["a", "ab", false], ["ab", "a", true],
                          ]);

var String_String_GE1 = (a, b) => a >= b;
warmup(String_String_GE1, [["", "", true], ["a", "a", true], ["aa", "aa", true],
                           ["a", "", true], ["", "a", false],
                           ["a", "b", false], ["b", "a", true],
                           ["a", "ab", false], ["ab", "a", true],
                          ]);

var String_String_LT1 = (a, b) => a < b;
warmup(String_String_LT1, [["", "", false], ["a", "a", false], ["aa", "aa", false],
                           ["a", "", false], ["", "a", true],
                           ["a", "b", true], ["b", "a", false],
                           ["a", "ab", true], ["ab", "a", false],
                          ]);

var String_String_LE1 = (a, b) => a <= b;
warmup(String_String_LE1, [["", "", true], ["a", "a", true], ["aa", "aa", true],
                           ["a", "", false], ["", "a", true],
                           ["a", "b", true], ["b", "a", false],
                           ["a", "ab", true], ["ab", "a", false],
                          ]);

var String_String_EQ1 = (a, b) => a == b;
warmup(String_String_EQ1, [["", "", true], ["a", "a", true], ["aa", "aa", true],
                           ["a", "", false], ["", "a", false],
                           ["a", "b", false], ["b", "a", false],
                           ["a", "ab", false], ["ab", "a", false],
                          ]);

var String_String_SEQ1 = (a, b) => a === b;
warmup(String_String_SEQ1, [["", "", true], ["a", "a", true], ["aa", "aa", true],
                            ["a", "", false], ["", "a", false],
                            ["a", "b", false], ["b", "a", false],
                            ["a", "ab", false], ["ab", "a", false],
                           ]);

var String_String_NE1 = (a, b) => a != b;
warmup(String_String_NE1, [["", "", false], ["a", "a", false], ["aa", "aa", false],
                           ["a", "", true], ["", "a", true],
                           ["a", "b", true], ["b", "a", true],
                           ["a", "ab", true], ["ab", "a", true],
                          ]);

var String_String_SNE1 = (a, b) => a !== b;
warmup(String_String_SNE1, [["", "", false], ["a", "a", false], ["aa", "aa", false],
                            ["a", "", true], ["", "a", true],
                            ["a", "b", true], ["b", "a", true],
                            ["a", "ab", true], ["ab", "a", true],
                           ]);

// IsHTMLDDA internal slot
// https://tc39.github.io/ecma262/#sec-IsHTMLDDA-internal-slot
var IsHTMLDDA_EQ1 = (a, b) => a == b;
warmup(IsHTMLDDA_EQ1, [[createIsHTMLDDA(), null, true],
                       [createIsHTMLDDA(), undefined, true],
                       [null, createIsHTMLDDA(), true],
                       [undefined, createIsHTMLDDA(), true],
                      ]);
