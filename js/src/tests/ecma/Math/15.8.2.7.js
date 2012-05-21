/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.7.js
   ECMA Section:       15.8.2.7 cos( x )
   Description:        return an approximation to the cosine of the
   argument.  argument is expressed in radians
   Author:             christine@netscape.com
   Date:               7 july 1997

*/

var SECTION = "15.8.2.7";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Math.cos(x)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.cos.length",
	      1,
	      Math.cos.length );

new TestCase( SECTION,
	      "Math.cos()",
	      Number.NaN,
	      Math.cos() );

new TestCase( SECTION,
	      "Math.cos(void 0)",
	      Number.NaN,
	      Math.cos(void 0) );

new TestCase( SECTION,
	      "Math.cos(false)",
	      1,
	      Math.cos(false) );

new TestCase( SECTION,
	      "Math.cos(null)",
	      1,
	      Math.cos(null) );

new TestCase( SECTION,
	      "Math.cos('0')",
	      1,
	      Math.cos('0') );

new TestCase( SECTION,
	      "Math.cos('Infinity')",
	      Number.NaN,
	      Math.cos("Infinity") );

new TestCase( SECTION,
	      "Math.cos('3.14159265359')",
	      -1,
	      Math.cos('3.14159265359') );

new TestCase( SECTION,
	      "Math.cos(NaN)",
	      Number.NaN,
	      Math.cos(Number.NaN)        );

new TestCase( SECTION,
	      "Math.cos(0)",
	      1,
	      Math.cos(0)                 );

new TestCase( SECTION,
	      "Math.cos(-0)",
	      1,
	      Math.cos(-0)                );

new TestCase( SECTION,
	      "Math.cos(Infinity)",
	      Number.NaN,
	      Math.cos(Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.cos(-Infinity)",
	      Number.NaN,
	      Math.cos(Number.NEGATIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.cos(0.7853981633974)",
	      0.7071067811865,
	      Math.cos(0.7853981633974)   );

new TestCase( SECTION,
	      "Math.cos(1.570796326795)",
	      0,
	      Math.cos(1.570796326795)    );

new TestCase( SECTION,
	      "Math.cos(2.356194490192)",
	      -0.7071067811865,
	      Math.cos(2.356194490192)    );

new TestCase( SECTION,
	      "Math.cos(3.14159265359)",
	      -1,
	      Math.cos(3.14159265359)     );

new TestCase( SECTION,
	      "Math.cos(3.926990816987)",
	      -0.7071067811865,
	      Math.cos(3.926990816987)    );

new TestCase( SECTION,
	      "Math.cos(4.712388980385)",
	      0,
	      Math.cos(4.712388980385)    );

new TestCase( SECTION,
	      "Math.cos(5.497787143782)",
	      0.7071067811865,
	      Math.cos(5.497787143782)    );

new TestCase( SECTION,
	      "Math.cos(Math.PI*2)",
	      1,
	      Math.cos(Math.PI*2)         );

new TestCase( SECTION,
	      "Math.cos(Math.PI/4)",
	      Math.SQRT2/2,
	      Math.cos(Math.PI/4)         );

new TestCase( SECTION,
	      "Math.cos(Math.PI/2)",
	      0,
	      Math.cos(Math.PI/2)         );

new TestCase( SECTION,
	      "Math.cos(3*Math.PI/4)",
	      -Math.SQRT2/2,
	      Math.cos(3*Math.PI/4)       );

new TestCase( SECTION,
	      "Math.cos(Math.PI)",
	      -1,
	      Math.cos(Math.PI)           );

new TestCase( SECTION,
	      "Math.cos(5*Math.PI/4)",
	      -Math.SQRT2/2,
	      Math.cos(5*Math.PI/4)       );

new TestCase( SECTION,
	      "Math.cos(3*Math.PI/2)",
	      0,
	      Math.cos(3*Math.PI/2)       );

new TestCase( SECTION,
	      "Math.cos(7*Math.PI/4)",
	      Math.SQRT2/2,
	      Math.cos(7*Math.PI/4)       );

new TestCase( SECTION,
	      "Math.cos(Math.PI*2)",
	      1,
	      Math.cos(2*Math.PI)         );

new TestCase( SECTION,
	      "Math.cos(-0.7853981633974)",
	      0.7071067811865,
	      Math.cos(-0.7853981633974)  );

new TestCase( SECTION,
	      "Math.cos(-1.570796326795)",
	      0,
	      Math.cos(-1.570796326795)   );

new TestCase( SECTION,
	      "Math.cos(-2.3561944901920)",
	      -.7071067811865,
	      Math.cos(2.3561944901920)   );

new TestCase( SECTION,
	      "Math.cos(-3.14159265359)",
	      -1,
	      Math.cos(3.14159265359)     );

new TestCase( SECTION,
	      "Math.cos(-3.926990816987)",
	      -0.7071067811865,
	      Math.cos(3.926990816987)    );

new TestCase( SECTION,
	      "Math.cos(-4.712388980385)",
	      0,
	      Math.cos(4.712388980385)    );

new TestCase( SECTION,
	      "Math.cos(-5.497787143782)",
	      0.7071067811865,
	      Math.cos(5.497787143782)    );

new TestCase( SECTION,
	      "Math.cos(-6.28318530718)",
	      1,
	      Math.cos(6.28318530718)     );

new TestCase( SECTION,
	      "Math.cos(-Math.PI/4)",
	      Math.SQRT2/2,
	      Math.cos(-Math.PI/4)        );

new TestCase( SECTION,
	      "Math.cos(-Math.PI/2)",
	      0,
	      Math.cos(-Math.PI/2)        );

new TestCase( SECTION,
	      "Math.cos(-3*Math.PI/4)",
	      -Math.SQRT2/2,
	      Math.cos(-3*Math.PI/4)      );

new TestCase( SECTION,
	      "Math.cos(-Math.PI)",
	      -1,
	      Math.cos(-Math.PI)          );

new TestCase( SECTION,
	      "Math.cos(-5*Math.PI/4)",
	      -Math.SQRT2/2,
	      Math.cos(-5*Math.PI/4)      );

new TestCase( SECTION,
	      "Math.cos(-3*Math.PI/2)",
	      0,
	      Math.cos(-3*Math.PI/2)      );

new TestCase( SECTION,
	      "Math.cos(-7*Math.PI/4)",
	      Math.SQRT2/2,
	      Math.cos(-7*Math.PI/4)      );

new TestCase( SECTION,
	      "Math.cos(-Math.PI*2)",
	      1,
	      Math.cos(-Math.PI*2)        );

test();
