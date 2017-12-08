/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommonn.org/licenses/publicdomain/
 */

/*
 * For the sake of cross compatibility with other implementations we
 * implement date parsing heuristics which support single and double
 * digit years. See bug: 1265136
 */

/**************
 * BEGIN TEST *
 **************/

for (let year of Array(100).keys()) {
    for (let month of Array(12).keys()) {
        for (let day of Array(31).keys()) {
            let fullYear = year >= 50 ? year + 1900 : year + 2000;
            let fullDate = new Date(`${month + 1}/${day + 1}/${fullYear}`);

            // mm/dd/yy
            let d1 = new Date(`${month + 1}/${day + 1}/${year}`);
            assertEq(d1.getTime(), fullDate.getTime())

            // yy/mm/dd
            let d2 = new Date(`${year}/${month + 1}/${day + 1}`);
            if (year > 31) {
                assertEq(d2.getTime(), fullDate.getTime())
            } else if (year > 12) {
                assertEq(d2.getTime(), new Date(NaN).getTime())
            }
        }
    }
}

assertEq(new Date("99/1/99").getTime(), new Date(NaN).getTime());
assertEq(new Date("13/13/13").getTime(), new Date(NaN).getTime());
assertEq(new Date("0/10/0").getTime(), new Date(NaN).getTime());

// Written months.
for (let year of Array(1000).keys()) {
    let fullDate = new Date(`5/1/${year}`);
    let d1 = new Date(`may ${year} 1`);
    let d2 = new Date(`may 1 ${year}`);

    let d3 = new Date(`1 may ${year}`);
    let d4 = new Date(`${year} may 1`);

    let d5 = new Date(`1 ${year} may`);
    let d6 = new Date(`${year} 1 may`);

    assertEq(d1.getTime(), fullDate.getTime())
    assertEq(d2.getTime(), fullDate.getTime())
    assertEq(d3.getTime(), fullDate.getTime())
    assertEq(d4.getTime(), fullDate.getTime())
    assertEq(d5.getTime(), fullDate.getTime())
    assertEq(d6.getTime(), fullDate.getTime())
}

assertEq(new Date("may 1999 1999").getTime(), new Date(NaN).getTime());
assertEq(new Date("may 0 0").getTime(), new Date(NaN).getTime());

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
