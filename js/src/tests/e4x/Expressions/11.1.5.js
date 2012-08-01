// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("11.1.5 - XMLList Initializer");

docfrag = <><name>Phil</name><age>35</age><hobby>skiing</hobby></>;
TEST(1, "xml", typeof(docfrag));

correct = <name>Phil</name>;
TEST(2, correct, docfrag[0]);

emplist = <>
          <employee id="0"><name>Jim</name><age>25</age></employee>
          <employee id="1"><name>Joe</name><age>20</age></employee>
          <employee id="2"><name>Sue</name><age>30</age></employee>
          </>; 
         
TEST(3, "xml", typeof(emplist));
TEST_XML(4, 2, emplist[2].@id);

END();
