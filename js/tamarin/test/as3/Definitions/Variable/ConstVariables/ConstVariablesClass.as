/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
package Package1
{
    public namespace ns1;
    
    public const packageItem1 = "const packageItem1 set at creation time";
    
    public const packageItem2 = "const packageItem2 set at creation time", packageItem3, packageItem4 = "const packageItem4 set at creation time";;
    
    public const packageItem5:int = 5;
    
    public class Class1
    {
        public const classItem1 = "const Class1 classItem1 set at creation time";
        public const classItem2 = "const Class1 classItem2 set at creation time", classItem3, classItem4 = "const Class1 classItem4 set at creation time";
        public const classItem5:int = 6;
        public static const classItem6 = "static const Class1 classItem6 set at creation time";
        ns1 const classItem7:String = "ns1 const Class1 classItem7 set at creation time";
        ns1 static const classItem8:String = "ns1 static const Class1 classItem8 set at creation time";
    }
    
    public class Class2
    {
        public const classItem1;
        public const classItem2, classItem3, classItem4;
        public const classItem5:int = 6;
        public static const classItem6 = init();
        ns1 const classItem7:String;
        ns1 static const classItem8:String = init2();
        
        public function Class2()
        {
            classItem1 = "const Class2 classItem1 set in constructor";
            classItem2 = "const Class2 classItem2 set in constructor";
            classItem4 = "const Class2 classItem4 set in constructor";
            classItem5 = 7;
            ns1::classItem7 = "ns1 const Class2 classItem7 set in constructor";
        }
        
        public static function init()
        {
            return "static const Class2 classItem6 set in function";
        }
        
        public static function init2()
        {
            return "ns1 static const Class2 classItem8 set in function";
        }
    }
}
