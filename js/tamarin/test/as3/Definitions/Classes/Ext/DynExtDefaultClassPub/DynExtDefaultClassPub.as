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
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
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




package DefaultClass {

    import DefaultClass.*
    
    dynamic class DynExtDefaultClassPubInner extends DefaultClass {

    // ************************************
    // access public method of parent
    // from default method of sub class
    // ************************************

    function subGetArray() : Array { return this.getPubArray(); }
    function subSetArray(a:Array) { this.setPubArray(a); }
    // function to test above from test scripts
    public function testSubArray(a:Array) : Array {
        this.subSetArray(a);
        return this.subGetArray();
    }
    
    // ************************************
    // access public method of parent
    // from public method of sub class
    // ************************************

    public function pubSubGetArray() : Array { return this.getPubArray(); }
    public function pubSubSetArray(a:Array) { this.setPubArray(a); }

    // ************************************
    // access public method of parent
    // from private method of sub class
    // ************************************

    private function privSubGetArray() : Array { return this.getPubArray(); }
    private function privSubSetArray(a:Array) { this.setPubArray(a); }

    // function to test above from test scripts
    public function testPrivSubArray(a:Array) : Array {
        this.privSubSetArray(a);
        return this.privSubGetArray();
    }

    // ************************************
    // access public method of parent
    // from final method of sub class
    // ************************************

    final function finSubGetArray() : Array { return this.getPubArray(); }
    final function finSubSetArray(a:Array) { this.setPubArray(a); }
    // function to test above from test scripts
    public function testFinSubArray(a:Array) : Array {
        this.finSubSetArray(a);
        return this.finSubGetArray();
    }
    
    // ************************************
    // access public method of parent
    // from public final method of sub class
    // ************************************

    public final function pubFinSubGetArray() : Array { return this.getPubArray(); }
    public final function pubFinSubSetArray(a:Array) { this.setPubArray(a); }
    // function to test above from test scripts
    public function testPubFinSubArray(a:Array) : Array {
        this.pubFinSubSetArray(a);
        return this.pubFinSubGetArray();
    }    
    
    // ************************************
    // access public method of parent
    // from private final method of sub class
    // ************************************

    private final function privFinSubGetArray() : Array { return this.getPubArray(); }
    private final function privFinSubSetArray(a:Array) { this.setPubArray(a); }
    // function to test above from test scripts
    public function testPrivFinSubArray(a:Array) : Array {
        this.privFinSubSetArray(a);
        return this.privFinSubGetArray();
    }       
    
    // ************************************
    // access public method of parent
    // from virtual method of sub class
    // ************************************

    virtual function virSubGetArray() : Array { return this.getPubArray(); }
    virtual function virSubSetArray(a:Array) { this.setPubArray(a); }
    // function to test above from test scripts
    public function testVirSubArray(a:Array) : Array {
        this.virSubSetArray(a);
        return this.virSubGetArray();
    }
    
     // ************************************
    // access public method of parent
    // from public virtual method of sub class
    // ************************************

    public virtual function pubVirSubGetArray() : Array { return this.getPubArray(); }
    public virtual function pubVirSubSetArray(a:Array) { this.setPubArray(a); }
    // function to test above from test scripts
    public function testPubVirSubArray(a:Array) : Array {
        this.pubVirSubSetArray(a);
        return this.pubVirSubGetArray();
    }  
    
     // ************************************
    // access public method of parent
    // from private virtual method of sub class
    // ************************************

    private virtual function privVirSubGetArray() : Array { return this.getPubArray(); }
    private virtual function privVirSubSetArray(a:Array) { this.setPubArray(a); }
    // function to test above from test scripts
    public function testPrivVirSubArray(a:Array) : Array {
        this.pubVirSubSetArray(a);
        return this.pubVirSubGetArray();
    }  
    
    // ***************************************
    // access public property from 
    // default method of sub class
    // ***************************************

    function subGetDPArray() : Array { return this.pubArray; }
    function subSetDPArray(a:Array) { this.pubArray = a; }
    // function to test above from test scripts
    public function testSubDPArray(a:Array) : Array {
        this.subSetDPArray(a);
        return this.subGetDPArray();
    }    
   
    // ***************************************
    // access public property from
    // public method of sub class
    // ***************************************

    public function pubSubGetDPArray() : Array { return this.pubArray; }
    public function pubSubSetDPArray(a:Array) { this.pubArray = a; }

    // ***************************************
    // access public property from
    // private method of sub class
    // ***************************************
 
    private function privSubGetDPArray() : Array { return this.pubArray; }
    private function privSubSetDPArray(a:Array) { this.pubArray = a; }
    // function to test above from test scripts
    public function testPrivDPArray(a:Array) : Array {
        this.privSubSetDPArray(a);
        return this.privSubGetDPArray();
    }  
    
    // ***************************************
    // access public property from 
    // final method of sub class
    // ***************************************

    final function finSubGetDPArray() : Array { return this.pubArray; }
    final function finSubSetDPArray(a:Array) { this.pubArray = a; }
    // function to test above from test scripts
    public function testFinDPArray(a:Array) : Array {
        this.finSubSetDPArray(a);
        return this.finSubGetDPArray();
    } 	

    // ***************************************
    // access public property from 
    // public final method of sub class
    // ***************************************

    public final function pubFinSubGetDPArray() : Array { return this.pubArray; }
    public final function pubFinSubSetDPArray(a:Array) { this.pubArray = a; }
    // function to test above from test scripts
    public function testPubFinDPArray(a:Array) : Array {
        this.pubFinSubSetDPArray(a);
        return this.pubFinSubGetDPArray();
    } 
    
    // ***************************************
    // access public property from 
    // private final method of sub class
    // ***************************************

    private final function privFinSubGetDPArray() : Array { return this.pubArray; }
    private final function privFinSubSetDPArray(a:Array) { this.pubArray = a; }
    // function to test above from test scripts
    public function testPrivFinDPArray(a:Array) : Array {
        this.privFinSubSetDPArray(a);
        return this.pubFinSubGetDPArray();
    } 	
    
        // ***************************************
        // access public property from 
        // virtual method of sub class
        // ***************************************
    
        virtual function virSubGetDPArray() : Array { return this.pubArray; }
        virtual function virSubSetDPArray(a:Array) { this.pubArray = a; }
        // function to test above from test scripts
        public function testVirDPArray(a:Array) : Array {
            this.virSubSetDPArray(a);
            return this.virSubGetDPArray();
        } 	
    
        // ***************************************
        // access public property from 
        // public final method of sub class
        // ***************************************
    
        public virtual function pubVirSubGetDPArray() : Array { return this.pubArray; }
        public virtual function pubVirSubSetDPArray(a:Array) { this.pubArray = a; }
        // function to test above from test scripts
        public function testPubVirDPArray(a:Array) : Array {
            this.pubFinSubSetDPArray(a);
            return this.pubFinSubGetDPArray();
        } 
        
        // ***************************************
        // access public property from 
        // private final method of sub class
        // ***************************************
    
        private virtual function privVirSubGetDPArray() : Array { return this.pubArray; }
        private virtual function privVirSubSetDPArray(a:Array) { this.pubArray = a; }
        // function to test above from test scripts
        public function testPrivVirDPArray(a:Array) : Array {
            this.privFinSubSetDPArray(a);
            return this.pubFinSubGetDPArray();
    }
	
	}
	// PUBLIC wrapper function for the dynamic class to be accessed;
	// otherwise it will give the error:
	// ReferenceError: DynExtDefaultClass is not defined
	//	at global$init()
	public class DynExtDefaultClassPub extends DynExtDefaultClassPubInner  {}

}
