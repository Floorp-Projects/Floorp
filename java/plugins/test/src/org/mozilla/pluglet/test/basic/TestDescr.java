/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */



package org.mozilla.pluglet.test.basic;

import java.util.*;
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;

public class TestDescr {

    public static String DELAY_PROP_NAME          = "DELAY";
    public static String RUN_STAGES_PROP_NAME     = "RUN_STAGES";
    public static String TEST_CLASS_PROP_NAME     = "TEST_CLASS";
    public static String ITERATOR_CLASS_PROP_NAME = "ITERATOR_CLASS";

    Class testClass = null;
    Class iteratorClass = null;
    Test testInstance = null;

    TestContext context = null;
    Vector runStages = new Vector();

    public TestDescr() {
    
    }
  
    public void initialize(Properties props, TestContext context){
        this.context = context;

        String stagesStr;
        stagesStr = props.getProperty(RUN_STAGES_PROP_NAME);
        if (stagesStr != null) {
            StringTokenizer st = new StringTokenizer(stagesStr);
            TestStage stage = null;
            System.out.println(stagesStr);
            while (st.hasMoreTokens()) {
                stage = TestStage.getStageByName(st.nextToken());
                if (stage != null) {
                    runStages.add(stage);                    
                } else {
                    ;//nb
                }
            }      
        } else {
            ;//nb
        }

        String testClassName = props.getProperty(TEST_CLASS_PROP_NAME);
        try {
            testClass = Class.forName(testClassName);
        } catch (Exception e) {
            e.printStackTrace();
            TestContext.registerFAILED("No test class name");
        }
        String iteratorClassName =  props.getProperty(ITERATOR_CLASS_PROP_NAME);
        if (iteratorClassName != null) {
            try {
                iteratorClass = Class.forName(iteratorClassName);
            } catch (Exception e) {
                TestContext.registerFAILED("Incorrectly specified iterator class name");
            }
            try {
                testInstance = new IterTest(testClass, (TestListIterator)iteratorClass.newInstance());
            } catch (Exception e) {
                e.printStackTrace();
                TestContext.registerFAILED("Cann't instantiate iterator class");
            }
        } else {
            try {
                testInstance = (Test)testClass.newInstance();
            } catch (Exception e) {
                e.printStackTrace();
                TestContext.registerFAILED("Cann't instantiate test class");
            } 
        }
    }

    public Test getTestInstance(TestStage stage) {
        //System.out.println("the stage value is "+stage.getName());
        if (runStages.indexOf(stage) > -1) {
            return testInstance;
        } else {
            return null;
        }
    }
}

class IterTest implements Test {
    Class testClass = null; 
    TestListIterator iterator = null;

    public IterTest(Class testClass, TestListIterator iterator) {
        this.testClass = testClass;
        this.iterator = iterator;
    }

    public void execute (TestContext context) {
        if (iterator.initialize(context)) {
            while(iterator.next()) {
                try{
                    ((Test)testClass.newInstance()).execute(context);
                    System.out.println("Executing");
                } catch (Exception e) {
                    e.printStackTrace();
                    TestContext.registerFAILED("Cann't instantiate/execute test");
                    break;
                }
            }
        } else {
            TestContext.registerFAILED("Cann't initialize iterator");
        }
    }

}
