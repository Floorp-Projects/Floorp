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

public class TestStage {
    
  public static String NO_ANY_STAGE = "NO_ANY_STAGE";
  public static String PLUGLET_INITIALIZE = "PLUGLET_INITIALIZE";
  public static String PLUGLET_CREATE_PLUGLET_INSTANCE = "PLUGLET_CREATE_PLUGLET_INSTANCE";
  public static String PLUGLET_SHUTDOWN = "PLUGLET_SHUTDOWN";
  public static String PLUGLET_INSTANCE_INITIALIZE = "PLUGLET_INSTANCE_INITIALIZE";
  public static String PLUGLET_INSTANCE_START = "PLUGLET_INSTANCE_START" ;
  public static String PLUGLET_INSTANCE_NEW_STREAM = "PLUGLET_INSTANCE_NEW_STREAM";
  public static String PLUGLET_INSTANCE_SET_WINDOW = "PLUGLET_INSTANCE_SET_WINDOW";
  public static String PLUGLET_INSTANCE_PRINT = "PLUGLET_INSTANCE_PRINT";
  public static String PLUGLET_INSTANCE_STOP = "PLUGLET_INSTANCE_STOP";
  public static String PLUGLET_INSTANCE_DESTROY = "PLUGLET_INSTANCE_DESTROY";
  public static String PLUGLET_STREAM_LISTENER_ON_START_BINDING = "PLUGLET_STREAM_LISTENER_ON_START_BINDING";
  public static String PLUGLET_STREAM_LISTENER_ON_DATA_AVAILABLE = "PLUGLET_STREAM_LISTENER_ON_DATA_AVAILABLE";
  public static String PLUGLET_STREAM_LISTENER_ON_FILE_AVAILABLE = "PLUGLET_STREAM_LISTENER_ON_FILE_AVAILABLE";
  public static String PLUGLET_STREAM_LISTENER_GET_STREAM_TYPE = "PLUGLET_STREAM_LISTENER_GET_STREAM_TYPE";
  public static String PLUGLET_STREAM_LISTENER_ON_STOP_BINDING = "PLUGLET_STREAM_LISTENER_ON_STOP_BINDING";
  public static String ANY = "ANY";

  static Hashtable names = new Hashtable();
  static {
      names.put(NO_ANY_STAGE, new TestStage(NO_ANY_STAGE));
      names.put(PLUGLET_INITIALIZE, new TestStage(PLUGLET_INITIALIZE));
      names.put(PLUGLET_CREATE_PLUGLET_INSTANCE, new TestStage(PLUGLET_CREATE_PLUGLET_INSTANCE));
      names.put(PLUGLET_SHUTDOWN, new TestStage(PLUGLET_SHUTDOWN));
      names.put(PLUGLET_INSTANCE_INITIALIZE, new TestStage(PLUGLET_INSTANCE_INITIALIZE));
      names.put(PLUGLET_INSTANCE_START, new TestStage(PLUGLET_INSTANCE_START));
      names.put(PLUGLET_INSTANCE_NEW_STREAM, new TestStage(PLUGLET_INSTANCE_NEW_STREAM));
      names.put(PLUGLET_INSTANCE_SET_WINDOW, new TestStage(PLUGLET_INSTANCE_SET_WINDOW));
      names.put(PLUGLET_INSTANCE_PRINT, new TestStage(PLUGLET_INSTANCE_PRINT));
      names.put(PLUGLET_INSTANCE_STOP, new TestStage(PLUGLET_INSTANCE_STOP));
      names.put(PLUGLET_INSTANCE_DESTROY, new TestStage(PLUGLET_INSTANCE_DESTROY));
      names.put(PLUGLET_STREAM_LISTENER_ON_START_BINDING, new TestStage(PLUGLET_STREAM_LISTENER_ON_START_BINDING));
      names.put(PLUGLET_STREAM_LISTENER_ON_DATA_AVAILABLE, new TestStage(PLUGLET_STREAM_LISTENER_ON_DATA_AVAILABLE));
      names.put(PLUGLET_STREAM_LISTENER_ON_FILE_AVAILABLE, new TestStage(PLUGLET_STREAM_LISTENER_ON_FILE_AVAILABLE));
      names.put(PLUGLET_STREAM_LISTENER_ON_FILE_AVAILABLE, new TestStage(PLUGLET_STREAM_LISTENER_ON_FILE_AVAILABLE));
      names.put(PLUGLET_STREAM_LISTENER_GET_STREAM_TYPE, new TestStage(PLUGLET_STREAM_LISTENER_GET_STREAM_TYPE));
      names.put(PLUGLET_STREAM_LISTENER_ON_STOP_BINDING, new TestStage(PLUGLET_STREAM_LISTENER_ON_STOP_BINDING));
      names.put(ANY, new TestStage(ANY));
  }

  private String id;

  private TestStage(String id) {
    this.id = id;
  }

  private String getId() {
    return id;
  }

  public String getName() {
    return id;
  }

  public static TestStage getStageById(String id) {
      return (TestStage)names.get(id);
  } 

  public static TestStage getStageByName(String name) {
      return (TestStage)names.get(name.toUpperCase());
  } 

  public boolean consist(TestStage stage) {

      if (stage.getId().equals(this.id)) {
          return true;
      } 
      if (stage.getId().equals(TestStage.ANY)) {
          return true;
      }
      return false;
  }

  public boolean equals(Object stage) {
      //System.out.println("to test "+this.id+" and "+((TestStage)stage).getId());
      if (stage != null) {
          if (((TestStage)stage).getId().equals(this.id)) {
              return true;
          } 
      }
      return false;
  }

}
