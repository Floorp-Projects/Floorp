/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript;

import java.util.Vector;
import com.ibm.bsf.*;
import com.ibm.bsf.util.*;

/**
 * Adapter for IBM's BSF (Bean Scripting Framework).
 * <p>
 * See http://www.mozilla.org/rhino/bsf.html. 
 * If you wish to build Rhino without this file, simply
 * remove it--the rest of Rhino does not depend upon it.
 */
public class BSFJavaScriptEngine extends BSFEngineImpl {

  /**
   * Initialize the engine.
   */
  public void initialize(BSFManager mgr, String lang,
                         Vector declaredBeans) 
      throws BSFException
  {
      super.initialize(mgr, lang, declaredBeans);
      if (!lang.equalsIgnoreCase("javascript")) {
          throw new BSFException(BSFException.REASON_UNKNOWN_LANGUAGE,
                                 "language " + lang + " unsupported");
      }
      Context cx = Context.enter();
      try { 
          scope = cx.initStandardObjects(new ImporterTopLevel());
          for (int i=0; i < declaredBeans.size(); i++) {
              BSFDeclaredBean bean = (BSFDeclaredBean) declaredBeans.elementAt(i);
              declareBean(bean);
          }
          BSFFunctions bsf = new BSFFunctions(mgr, this);
          scope.put("bsf", scope, cx.toObject(bsf, scope));
      } finally {
          Context.exit();
      }
  }
             

  /**
   * Call a JavaScript method.
   * @param object a JavaScript object.
   * @param name the name of the method
   * @param args the arguments to 
   */
  public Object call(Object object, String name, Object[] args) 
      throws BSFException
  {
      if (object == null)
          object = scope;
      if (!(object instanceof Scriptable)) {
          throw new BSFException(BSFException.REASON_INVALID_ARGUMENT, 
                                 object.toString() + 
                                 " is not a JavaScript object");
      }
      try {
          return unwrap(ScriptableObject.callMethod((Scriptable) object, 
                                                    name, args));
      } catch (JavaScriptException e) {
          throw createBSFException(e);
      }
  }
  
  /**
   * Evaluate a JavaScript expression.
   * <p>
   * @param source the JavaScript source
   * @param lineNo the starting line number 
   * @param columnNo the starting column number (currently ignored)
   * @param expr the JavaScript expression to evaluate
   * @returns the result of the expression
   */
  public Object eval(String sourceName, int lineNo, int columnNo, Object expr) 
      throws BSFException
  {
      Context cx = Context.enter();
      try {
          // interpretive mode is faster for one-shot evaluations
          cx.setOptimizationLevel(-1);
          return unwrap(cx.evaluateString(scope, expr.toString(),
                                          sourceName, lineNo, null));
      } catch (JavaScriptException e) {
          throw createBSFException(e);
      } finally {
          Context.exit();
      }
  }

  /**
   * Declare a bean after the engine has been started. Declared beans
   * are beans that are named and which the engine must make available
   * to the scripts it runs in the most first class way possible.
   *
   * @param bean the bean to declare
   *
   * @exception BSFException if the engine cannot do this operation
   */
  public void declareBean(BSFDeclaredBean bean) 
      throws BSFException
  {
      scope.put(bean.name, scope, Context.toObject(bean.bean, scope));
  }

  /**
   * Undeclare a previously declared bean.
   *
   * @param bean the bean to undeclare
   *
   * @exception BSFException if the engine cannot do this operation
   */
  public void undeclareBean(BSFDeclaredBean bean) 
      throws BSFException
  {
      scope.delete(bean.name);
  }
  
  private Object unwrap(Object obj) {
      if (obj instanceof Wrapper)
          return ((Wrapper) obj).unwrap();
      return obj;
  }
  
  private BSFException createBSFException(JavaScriptException e) 
      throws BSFException
  {
      Object obj = e.getValue();
      obj = unwrap(obj);
      if (obj instanceof Throwable) {
          return new BSFException(BSFException.REASON_EXECUTION_ERROR,
                                  e.toString(), (Throwable) obj);
      } else {
          return new BSFException(BSFException.REASON_EXECUTION_ERROR,
                                  e.toString());
      }
  }

  private Scriptable scope;
}
