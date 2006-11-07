/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

package es4
{
    import avmplus.File

    // shell

    public function evalFile(src_name)
    {
        var base_name = src_name.split('.')[0]
        var abc_name = base_name+".abc"
        var src = File.read(src_name)
		return evalString(src)
    }

    public function evalString(src)
    {
        var evaluator = new Evaluator
        try 
		{
            var result = evaluator.eval(src)
        }
        catch(x)
        {
            print("\n**" + x + "**")
			var result = <LiteralString value=""/>
        }
        return result
    }
}

include "debugger.as"
include "parser.as"
include "definer.as"
include "emitter.as"
include "interpreter.as"
include "evaluator.as"

{
    import avmplus.System
    import avmplus.File
    import es4.*
    
    if( System.argv.length < 1 ) 
    {
	    print(File.read("es4_start.txt"))
        while( true )
        {
            System.write("es4>")
            var s = System.readLine()
            if( s == ".quit" ) break
            else if( s.indexOf(".help") == 0 )
			{
			    print(File.read("es4_start.txt"))
			    print(File.read("es4_help.txt"))
			}
            else if( s.indexOf(".trace") == 0 )
			{
				var arg_name = s.substr(7)
				if( arg_name == "on" )
				{
					es4.log_mode = debug
					print("tracing on")
				}
				else
				{
					es4.log_mode = release
					print("tracing off")
				}
			}
            else if( s.indexOf(".load") == 0 )
			{
				var arg_name = s.substr(6)
		        var result = evalFile(arg_name)
				if( result != void 0 )
				{
					print(result)
				}
			}
            else if( s.indexOf(".test") == 0 )
			{
				var result = testABCEmitter()
				if( result != void 0 )
				{
					print(result)
				}
			}
			else
			{
	            var result = evalString(s)
				if( result != void 0 )
				{
					print(result)
				}
			}
        }
    }
    else
    {
        print(System.argv[0])
        var result = evalFile(System.argv[0])
		print(result)
    }
}

