// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.8 - XMLList copy()");

TEST(1, true, XMLList.prototype.hasOwnProperty("copy"));

emps = new XMLList();
emps += <employee id="0"><name>Jim</name><age>25</age></employee>;
emps += <employee id="1"><name>Joe</name><age>20</age></employee>;

correct = new XMLList();
correct += <employee id="0"><name>Jim</name><age>25</age></employee>;
correct += <employee id="1"><name>Joe</name><age>20</age></employee>;

TEST(2, emps, emps.copy());
TEST(3, correct, emps.copy());

// Make sure we're getting a copy, not a ref to orig.
emps = new XMLList();
emps += <employee id="0"><name>Jim</name><age>25</age></employee>;
emps += <employee id="1"><name>Joe</name><age>20</age></employee>;
  
correct = new XMLList();
correct += <employee id="0"><name>Jim</name><age>25</age></employee>;
correct += <employee id="1"><name>Joe</name><age>20</age></employee>;

x = emps.copy();

emps += <employee id="2"><name>Sue</name><age>32</age></employee>;

TEST(4, correct, x);

END();
