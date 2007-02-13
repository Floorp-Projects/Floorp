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

import avmplus.*

function assert(b) { if (!b) {throw new Error("Asserted");} }

if (System.argv.length<1)
{
	print("You need to specific at least one xml file");
	System.exit(1);
}

// parse / function call tables 
var op_table:Array = [];
var operator:Array = [];
initTables();

// expression stacks used while parsing
var tStack:Array = [];
var exprStack:Array = [];

// main
var contexts:Array = [];
var currentOutput:String = "";
var showStatus:Boolean = false;
var debug:Boolean = false;
var i:int = 0;
while(i < System.argv.length)
{
	if (System.argv[i].charAt(0) == "+")
	{
		// option paramters
		if (System.argv[i]=="+s")
			showStatus = true;
		else if (System.argv[i]=="+d")
			debug = true;
		i++;
		continue;
	}

	var context:XML = <x file={System.argv[i++]} this_file='trn.as' />;
	load(context);
	contexts.push(context);

	// generate 
	for each (var item:XML in context..Generate.children())
	{
		var name = item.@name;
		if (item.@disable == "true")
			continue;

		var content:String = express(item);

		if (item.name() == "File")
			File.write(name, content);
		else
			print(content);
	}
}

// create and push a new context node for any loops i.e. 'repeats'
function acquireContext():XML
{
	var ctx:XML = <context/>;
	contexts.push(ctx);
	return ctx;
}

function releaseContext(ctx:XML)
{
	assert(ctx == contexts[contexts.length-1]); // pop them off in order please
	contexts.pop();
}

function repeat(x:XML):String
{
	var repeatName:String = x.attribute("repeat");
	var mergeName:String =  x.attribute("mergeWith");
	var usedMergeName:String = mergeName;
	var skip:Boolean = (x.attribute("skipUnattributed") == "false") ? false : true;
	var useOrig:Boolean = (x.attribute("useOriginalIfMergeFail") == "false") ? false : true;

	var over:XML = lookup( parseVar(repeatName) );
	var mergeWith:XML = lookup( parseVar(mergeName) );

	var mergeAttr:String = (mergeWith != null) ? x.attribute("mergeAttribute") : null;

	if (over==null)
		print("ERROR: in repeat attribute, '"+repeatName+"' not found");

	var n:String = "";
	var count:int = 0;

	var us:XML = parse(x.toString());
//	print("repeat "+us+" on "+x.toString()+" named "+repeatName+" over "+over.children().toXMLString());
	// set up a new context for lookups
	var ctx:XML = acquireContext();

	// use pseudo name for merge node	
	mergeName = "merge";

	// put parent in current context 
	x.prependChild(<count>{count}</count>);
	ctx.prependChild(x);
	if (mergeWith != null) ctx.prependChild(<{mergeName}/>);

	for each (var item:XML in over.children())
	{
		if (skip && String(item.attributes()).length < 1)
			continue;  // skip un-attributed nodes

		// make the merge node available
		if (mergeWith != null)
		{
			var node = childMatchingAttribute(mergeWith, String(mergeAttr), String(item.@[mergeAttr]));
			if (node != null)
				ctx[mergeName].appendChild(node);
			else
			{
				print("Warning: no node with "+String(mergeAttr)+" = '"+String(item.@[mergeAttr])+"' in "+usedMergeName+" while merging with "+repeatName);
				if (useOrig)
				{
					ctx[mergeName].appendChild(item[0]);
					if (showStatus) print("Using "+item.toXMLString());
				}
			}
		}

		// make nodes visible in our context
		x.replace("count", <count>{count}</count>);
		ctx.prependChild(item);

		if (debug) print("Context pre-resolve "+ctx.toXMLString());

		// now that our state is current we can expand the text of the node.
		n += resolve(us);
		count++;

		delete ctx.*[0];
		if (mergeWith != null) delete ctx[mergeName].*;

		if (debug) print("Context post-resolve "+ctx.toXMLString());
	}

	// trim the output?
	if (x.attribute("trim") > 0)
		n = n.slice(0, n.length-Number(x.attribute("trim")));

	// make repeat count available in 'global'
	delete contexts[0].*[0].last_repeat_count;
	contexts[0].*[0].prependChild(<last_repeat_count>{count}</last_repeat_count>);

	delete x.count;
	releaseContext(ctx);
	return n;
}

function childMatchingAttribute(x:XML, attrName:String, attrValue:String):XML
{
	// find a child that has an attribute of a specific value
	for each (var item in x.children())
	{	
		var val = item.@[attrName];
		if (val)
		{
			if (String(val) == attrValue)
				return item;
		}
	}	
	return null;
}


function lookup(r:XML):XML
{
	if (r == null)
		return null;

	// resolve lhs
	var arr:Array = argsFor(r, "__dot");
	var x:XML = arr.pop();
	var ref:String = evaluate(x.*[0]);
	var ctx = contextFor(ref);
	var name:String = ref;
	do 
	{
		ref = evaluate(x.*[1]);
		assert(x.*[1].name() != "__dot");  // shouldn't recurse in lookup()
		name += "."+ref;
		ctx = ctx[ref];
		x = arr.pop();
	} 
	while(x != null && ctx != null);

	if (ctx == null || ctx.length() == 0)
		print("Warning: Could not find '"+name+"'");
	else if (ctx instanceof XMLList)
		ctx = ctx[0];

	return ctx;
}

function argsFor(r:XML, tag:String="__comma"):Array
{
	var arr:Array = [];
	while(r.name() == tag)
	{
		arr.push(r);
		r = r.*[0];
	}
	return arr;
}

function contextFor(s:String):XML
{
	if (s == null || s.length == 0)
		return null;

	// try each context
	var x = null;
	var len:int = contexts.length;
	for(var i:int=len-1; i>-1; i--)
	{
		var context = contexts[i];
		if (s == "*" || (context[s].length() == 1 && context[s].name() == s))
		{
			x = context[s];
			break;
		}
	}

	if (x == null)
		print("ERROR: Can't find xml node "+s);
 
	if (x instanceof XMLList)
		x = x[0];

	return x;
}

function express(n:XML):String
{
	if (debug) print("expressing '"+n+"'");

	// special processing directives?
	contexts.push(<contexts>{n}</contexts>);
	var s:String = "";
	if (n.attribute("repeat").length() > 0)
		s = repeat(n);
	else
	{
		var x:XML = parse(n.toString());
		s = resolve(x);
	}
	contexts.pop();
	return s;
}

function resolve(r:XML):String
{
	var s:String = "";
	for each(var item:XML in r.children())
	{
		s += evaluate(item);
		currentOutput = s;
	}
	return s;
}

function evaluate(r:XML):String
{
	var s:String = "";
	var f = op_table[r.name()];
	if (f == null)
		print("Warning: function '"+r.name()+"' unknown for "+r.toXMLString());
	else
		s = f.call(this, r);
	return s;
}

function parse(s:String):XML
{
	var subs:String = "";
	var len:int = s.length;
	var tree:XML = <root/>;
	for(var i:int=0; i<len; i++)
	{
		var ch:String = s.charAt(i);
		var n:XML = opFor(ch+s.charAt(i+1));	
		if (n != null && n.name() == "__OpenExpr")
		{
			appendConst(tree, subs);
			addOp(n);
			i = parseExpr(s, i+2, len);
			tree.appendChild( exprStack.pop() );
			assert(exprStack.length == 0 && tStack.length == 0);
			subs = "";
		}
		else
			subs += ch;
	}

	appendConst(tree, subs);
	return tree;
}

function parseVar(s:String):XML
{
	var len:int = s.length;
	if (len<1)
		return null;
	addOp( opFor("{{") );
	parseExpr(s, 0, len);
	addOp( opFor("}}") ); // force expression to resolve fully 
	assert(exprStack.length == 1 && tStack.length == 0);
	return exprStack.pop().*[0];
}

function parseExpr(s:String, start:int, max:int):int
{
	var subs:String = "";
	for(var i:int=start; i<max; i++)
	{
		var ch:String = s.charAt(i);
		if (isWhitespace(ch))
 		{
			addOperand(subs);
			subs = "";
			continue;
		}
		else if (ch == "&")
		{
			// xml escape chars
			var at:int = i;
			while(s.charAt(i++) != ";")
				;
			var x = new XML(s.substring(at,i));
			ch = x.toString();
			subs += ch;
			continue;
		}

		var n:XML = opFor(ch+s.charAt(i+1));	
		if (n == null)
			subs += ch;
		else
		{
			addOperand(subs);
			subs = "";

			addOp(n);
			if (n.attribute("twoToken") == "true")
				i++; // consumed 2 chars?

			if (n.name() == "CloseExpr")
				break;
		}				
	}

	addOperand(subs);
	return i;
}

function popOperation()
{
	var op:XML = tStack.pop();
	op.prependChild( exprStack.pop() );

	if (op.attribute("singleOperand") != "true" )
		op.prependChild( exprStack.pop() );
	exprStack.push(op);
}

function addOp(op:XML)
{
	if (String(op.name()).indexOf("__Open") == 0)
		tStack.push(op);  // marker with low precedence forces pops when close is hit
	else
	{
		var opPrec = op.attribute("prec");
		while( (tStack.length>1) && (tStack[tStack.length-1].attribute("prec") >= opPrec) )
			popOperation();

		if (String(op.name()).indexOf("Close") == 0)
		{
			// we stopped popping up to the close, so we need to peel off the open which is tos
			popOperation();

			// mondo hack that causes issues with expressions such as g + (2) but
			// allows us to easily support function-like notation such as g(2)
			var at = exprStack.length-2;
			if ( (at>-1) && (exprStack[at].name() == "__oprnd") && !Number(exprStack[at]) )
			{
				tStack.push( exprStack.pop() );
//				popOperation();
				var nm:XML = exprStack.pop();
				var name:String = "__" + nm.toString();
				tStack[tStack.length-1].setName(name);
				exprStack.push( tStack.pop() );
			}
		}
		else
		{
			tStack.push(op);
		}
	}
}

function opFor(c:String)
{
	var op:XML = operator[c];
	if (op == null)
		op = operator[c.charAt(0)];

	if (op != null && op.name() == "__ignore")
		op = null;
	return (op == null) ? null : new XML(op);
}

function addOperand(s:String)
{
	if (s.length>0)
		exprStack.push( <__oprnd prec='18' >{s}</__oprnd> );
}

function appendConst(tree:XML, s:String)
{
	XML.ignoreWhitespace = false;
	if (s.length>0)
	{
		var str = <__str/>;
		str.appendChild( new XML("<![CDATA["+s+"]]>") );
		tree.appendChild( str );
	}
	XML.ignoreWhitespace = true;
}

function isWhitespace(c:String)
{
	var itIs:Boolean = false
	if ( (c == " ") || (c == "\r") || (c == "\n") || (c == "\t") )
		itIs = true;
	return itIs;
}

function shows(s:String=""):String
{
	for(var i:int=0; i<tStack.length;i++)
		s += "  tStack["+i+"] = "+tStack[i].toXMLString()+"\n" ;
	for(var i:int=0; i<exprStack.length;i++)
		s += "  exprStack["+i+"] = "+exprStack[i].toXMLString()+"\n" ;
	return s;
}

function load(x:XML)
{
	if (x == null)
		return;

	// children reside in another file?
	if (x.attribute("file").length() > 0)
	{
		if (x.attribute("loaded") == 'true')
			return;

		var fn = x.attribute("file");
		if (showStatus) print("Loading "+fn);
		var c:XML = XML(File.read(fn));
		if (c.toXMLString().length == 0)
		{
			print("ERROR: Unable to find file "+fn);
			System.exit(2);
		}

		x.@loaded='true';
		x.appendChild(c);
	}

	// expand children
	for each (var item in x.children())
		load(item);
}

// ops

function f__OpenParen(x:XML)					{ assert(x.*[1] == null); return String(evaluate(x.*[0])); }
function f__OpenExpr(x:XML)						{ assert(x.*[1] == null); return String(evaluate(x.*[0])); }
function f__comma(x:XML)						{ assert(false); }
function f__Assignment(x:XML)					{ assert(false); } //String( evaluate(x.*[0]) = evaluate(x.*[1]) ); }
function f__LogicOr(x:XML)						{ return String( Boolean(evaluate(x.*[0])) || Boolean(evaluate(x.*[1])) ); }
function f__LogicAnd(x:XML)						{ return String( Boolean(evaluate(x.*[0])) && Boolean(evaluate(x.*[1])) ); }
function f__Or(x:XML)							{ return String( Number(evaluate(x.*[0]))  |  Number(evaluate(x.*[1])) ); }
function f__Xor(x:XML)							{ return String( Number(evaluate(x.*[0]))  ^  Number(evaluate(x.*[1])) ); }
function f__And(x:XML)							{ return String( Number(evaluate(x.*[0]))  &  Number(evaluate(x.*[1])) ); }
function f__Eq(x:XML)							{ return String( Boolean(evaluate(x.*[0])) == Boolean(evaluate(x.*[1])) ); }
function f__Neq(x:XML)							{ return String( Boolean(evaluate(x.*[0])) != Boolean(evaluate(x.*[1])) ); }
function f__LT(x:XML)							{ return String( Boolean(evaluate(x.*[0])) <  Boolean(evaluate(x.*[1])) ); }
function f__GT(x:XML)							{ return String( Boolean(evaluate(x.*[0])) >  Boolean(evaluate(x.*[1])) ); }
function f__LTEq(x:XML)							{ return String( Boolean(evaluate(x.*[0])) <= Boolean(evaluate(x.*[1])) ); }
function f__GTEq(x:XML)							{ return String( Boolean(evaluate(x.*[0])) >= Boolean(evaluate(x.*[1])) ); }
function f__LShift(x:XML)						{ return String( Number(evaluate(x.*[0]))  << Number(evaluate(x.*[1])) ); }
function f__RShift(x:XML)						{ return String( Number(evaluate(x.*[0]))  >> Number(evaluate(x.*[1])) ); }
function f__Add(x:XML)							{ return String( Number(evaluate(x.*[0]))  +  Number(evaluate(x.*[1])) ); }
function f__Sub(x:XML)							{ return String( Number(evaluate(x.*[0]))  -  Number(evaluate(x.*[1])) ); }
function f__Mult(x:XML)							{ return String( Number(evaluate(x.*[0]))  *  Number(evaluate(x.*[1])) ); }
function f__Div(x:XML)							{ return String( Number(evaluate(x.*[0]))  /  Number(evaluate(x.*[1])) ); }
function f__Mod(x:XML)							{ return String( Number(evaluate(x.*[0]))  %  Number(evaluate(x.*[1])) ); }
function f__LogicNot(x:XML)						{ return String( !Boolean(evaluate(x.*[0])) ); }
function f__BitNot(x:XML)						{ return String( ~Number(evaluate(x.*[0])) ); }
function f__array(x:XML)						{ return String(x); }
function f__str(x:XML):String					{ return String(x); }
function f__strlen(x:XML):String				{ return String ( evaluate(x.*[0]).length ); }

function initTables()
{
	operator["tree"] = <tree prec='-15' singleOperand='true'/>; 			// fake node which forces expressions to resolve 
	operator["}}"] = <CloseExpr view='}}' prec='-10' singleOperand='true' twoToken='true'/>;
	operator["{{"] = <__OpenExpr view='}}' prec='-9' singleOperand='true' twoToken='true'/>;

	operator[")"] = <CloseParen view=')' prec='-2' singleOperand='true' />;
	operator["("] = <__OpenParen view='(' prec='-1'  singleOperand='true' />;

	operator["]"] = <CloseBracket view=']' prec='-2' singleOperand='true'/>
	operator["["] = <__OpenBracket view='[' prec='-1' singleOperand='true' />

	operator[","] = <__comma view=',' prec='1'/>;
	operator["="] = <__Assignment view='=' prec='2'/>;
	operator["||"] = <__LogicOr view='||' prec ='4' twoToken='true'/>;
	operator["&&"] = <__LogicAnd view='&&' prec='5' twoToken='true'/>;
	operator["|"] = <__Or view='|' prec='6'/>;
	operator["^"] = <__Xor view='^' prec='7'/>;
	operator["&"] = <__And view='&' prec='8'/>;
	operator["=="] = <__Eq view='==' prec='9' twoToken='true'/>;
	operator["!="] = <__Neq view='!=' prec='9' twoToken='true'/>;
	operator["<"] = <__LT view='&lt;' prec='10'/>;
	operator[">"] = <__GT view='&gt;' prec='10'/>;
	operator["<="] = <__LTEq view='&lt;=' prec='10' twoToken='true'/>;
	operator[">="] = <__GTEq view='&gt;=' prec='10' twoToken='true'/>;
	operator["<<"] = <__LShift view='&lt;&lt;' prec='11' twoToken='true'/>;
	operator[">>"] = <__RShift view='&gt;&gt;' prec='11' twoToken='true'/>;
	operator["+"] = <__Add view='+' prec='12'/>;
	operator["-"] = <__Sub view='-' prec='12'/>;
	operator["*"] = <__Mult view='*' prec='13'/>;
	operator["/"] = <__Div view='/' prec='13'/>;
	operator["%"] = <__Mod view='%' prec='13'/>;
	operator["!"] = <__LogicNot view='!' prec='15' singleOperand='true'/>;
	operator["~"] = <__BitNot view='~' prec='15' singleOperand='true'/>;
	operator["."] = <__dot view='.' prec='17'/>;
	operator["array"] = <array view='array' prec='19'/>;
	operator["*."] = <__ignore />; // allow * in dot expressions

	// functions
	op_table["__OpenExpr"] = f__OpenExpr;
	op_table["__OpenParen"] = f__OpenParen;
//	op_table["__OpenBracket"] = f__OpenBracket;
	op_table["__comma"] = f__comma;
	op_table["__Assignment"] = null; //f__Assignment;
	op_table["__LogicOr"] = f__LogicOr;
	op_table["__LogicAnd"] = f__LogicAnd;
	op_table["__Or"] = f__Or;
	op_table["__Xor"] = f__Xor;
	op_table["__And"] = f__And;
	op_table["__Eq"] = f__Eq;
	op_table["__Neq"] = f__Neq;
	op_table["__LT"] = f__LT;
	op_table["__GT"] = f__GT;
	op_table["__LTEq"] = f__LTEq;
	op_table["__GTEq"] = f__GTEq;
	op_table["__LShift"] = f__LShift;
	op_table["__RShift"] = f__RShift;
	op_table["__Add"] = f__Add;
	op_table["__Sub"] = f__Sub;
	op_table["__Mult"] = f__Mult;
	op_table["__Div"] = f__Div;
	op_table["__Mod"] = f__Mod;
	op_table["__LogicNot"] = f__LogicNot;
	op_table["__BitNot"] = f__BitNot;
	op_table["__dot"] = f__dot;
	op_table["__array"] = f__array;
	op_table["__str"] = f__str;
	op_table["__oprnd"] = f__oprnd;
	op_table["__strlen"] = f__strlen;
	op_table["__substring"] = f__substring;
	op_table["__escape"] = f__escape;
	op_table["__spaces"] = f__spaces;
	op_table["__column"] = f__column;
	op_table["__context"] = f__context;
	op_table["__parseTree"] = f__parseTree;
	op_table["__showTree"] = f__showTree;
}

function f__dot(x:XML):String
{
	var r = lookup(x);
	var s:String = "";

	if (r == null)
		s = ""; // lookup has already issued a warning 
	else if (r instanceof XML)
		s = express(r);
	else
		s = String(r);
	return s;
}

function f__oprnd(x:XML):String
{ 
	var s:String = String(x);
	return s;
}

function f__escape(x:XML):String
{
	var args = argsFor(x.*[0]);
	var charsToEscape:String = "\"";
	var withSeq:String = "\\\"";
	var arg0:XML = x.*[0];
	if (args.length>1)
	{
		// three args or 1 arg
		charsToEscape  = evaluate(args[1].*[1]);
		withSeq =  evaluate(args[0].*[1]);	
		arg0 = args[1].*[0];
	}
	var s = evaluate(arg0);
	var n:String = "";
	var len:int = s.length;
	for(var i:int=0; i<len; i++)
	{
		var c = s.charAt(i);
		if (c == "\\")	
		{
			c = String.fromCharCode((64*s.charAt(i+1))+(8*s.charAt(i+2))+s.charAt(i+2));
			i += 3;
		}
		else if (charsToEscape.indexOf(c) > -1)
			n += withSeq;
		else
			n += c;
	}
	return n;
}

function f__substring(x:XML):String				
{
	// <substring><comma><comma><arg0/><arg1></comma><arg2></comma></substring>
	var args = argsFor(x.*[0]);
	if (args.length<1)
	{
		print("WARNING: substring requires at least 1 argument");	
		return "";
	}
	else if (args.length<2)
	{
		var from = Number( evaluate(args[0].*[1]) );
		return evaluate(args[0].*[0]).substring(from);
	}
	else
	{
		var from = Number( evaluate(args[1].*[1]) );
		var to = Number ( evaluate(args[0].*[1]) );
		return evaluate(args[1].*[0]).substring(from, to);
	}
}

function f__spaces(x:XML):String
{
	var c:Number = Number( evaluate(x.*[0]) );
	var n:String="";
	while(c-->0)
		n+= " ";
	return n;
}

function f__column(x:XML):String
{
	var num:Number = Number( evaluate(x.*[0]) );

	// lets figure out which column we're at and the appropriate amount of padding
	var cr:int = currentOutput.lastIndexOf("\n");
	var c:int = num-(currentOutput.length-int(cr)-1);
	var n:String="";
	while(c-->0)
		n+= " ";
	return n;
}

function f__context(x:XML):String
{
	var num:Number = Number( evaluate(x.*[0]) );
	if (isNaN(num) || (num<0) || (num>(contexts.length-1)) )
		num = contexts.length-1;
	return contexts[num].toString();
}

function f__parseTree(x:XML):String
{
	return x.toString();
}

function f__showTree(x:XML, d:int=0):String
{
	var s:String="";
	if (x.children().length() > 0)
	{
		var paren:Boolean = x.attribute("view")  == "(" ? true : false;
		var len:int  = x.children().length();
		if (len == 1)
			s = f__showTree(x.*[0]);
		else if (len == 2)
			s = f__showTree(x.*[0]) + x.attribute("view") + f__showTree(x.*[1]);
		else
		{
			for(var i:int=0; i<len; i++)
				s += f__showTree(x.*[i]);
		}
		if (paren)
			s += ") ";
	}
	else 
	{
		s = x.toString();
	}
	return s;
}
