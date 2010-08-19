/*
 * Check that only JS_REQUIRES_STACK/JS_FORCES_STACK functions, and functions
 * that have called a JS_FORCES_STACK function, access cx->fp directly or
 * indirectly.
 */

require({ after_gcc_pass: 'cfg' });
include('gcc_util.js');
include('unstable/adts.js');
include('unstable/analysis.js');
include('unstable/lazy_types.js');
include('unstable/esp.js');

var Zero_NonZero = {};
include('unstable/zero_nonzero.js', Zero_NonZero);

// Tell MapFactory we don't need multimaps (a speed optimization).
MapFactory.use_injective = true;

/*
 * There are two regions in the program: RED and GREEN.  Functions and member
 * variables may be declared RED in the C++ source.  GREEN is the default.
 *
 * RED signals danger.  A GREEN part of a function must not call a RED function
 * or access a RED member.
 *
 * The body of a RED function is all red.  The body of a GREEN function is all
 * GREEN by default, but parts dominated by a call to a TURN_RED function are
 * red.  This way GREEN functions can safely access RED stuff by calling a
 * TURN_RED function as preparation.
 *
 * The analysis does not attempt to prove anything about the body of a TURN_RED
 * function.  (Both annotations are trusted; only unannotated code is checked
 * for errors.)
 */
const RED = 'JS_REQUIRES_STACK';
const TURN_RED = 'JS_FORCES_STACK';
const IGNORE_ERRORS = 'JS_IGNORE_STACK';

function attrs(tree) {
  let a = DECL_P(tree) ? DECL_ATTRIBUTES(tree) : TYPE_ATTRIBUTES(tree);
  return translate_attributes(a);
}

function hasUserAttribute(tree, attrname) {
  let attributes = attrs(tree);
  if (attributes) {
    for (let i = 0; i < attributes.length; i++) {
      let attr = attributes[i];
      if (attr.name == 'user' && attr.value.length == 1 && attr.value[0] == attrname)
        return true;
    }
  }
  return false;
}

function process_tree_type(d)
{
  let t = dehydra_convert(d);
  if (t.isFunction)
    return;

  if (t.typedef !== undefined)
    if (isRed(TYPE_NAME(d)))
      warning("Typedef declaration is annotated JS_REQUIRES_STACK: the annotation should be on the type itself", t.loc);
  
  if (hasAttribute(t, RED)) {
    warning("Non-function is annotated JS_REQUIRES_STACK", t.loc);
    return;
  }
  
  for (let st = t; st !== undefined && st.isPointer; st = st.type) {
    if (hasAttribute(st, RED)) {
      warning("Non-function is annotated JS_REQUIRES_STACK", t.loc);
      return;
    }
    
    if (st.parameters)
      return;
  }
}

function process_tree_decl(d)
{
  // For VAR_DECLs, walk the DECL_INITIAL looking for bad assignments
  if (TREE_CODE(d) != VAR_DECL)
    return;
  
  let i = DECL_INITIAL(d);
  if (!i)
    return;
  
  assignCheck(i, TREE_TYPE(d), function() { return location_of(d); });

  functionPointerWalk(i, d);
}

/*
 * x is an expression or decl.  These functions assume that 
 */
function isRed(x) { return hasUserAttribute(x, RED); }
function isTurnRed(x) { return hasUserAttribute(x, TURN_RED); }

function process_tree(fndecl)
{
  if (hasUserAttribute(fndecl, IGNORE_ERRORS))
    return;

  if (!(isRed(fndecl) || isTurnRed(fndecl))) {
    // Ordinarily a user of ESP runs the analysis, then generates output based
    // on the results.  But in our case (a) we need sub-basic-block resolution,
    // which ESP doesn't keep; (b) it so happens that even though ESP can
    // iterate over blocks multiple times, in our case that won't cause
    // spurious output.  (It could cause us to the same error message each time
    // through--but that's easily avoided.)  Therefore we generate the output
    // while the ESP analysis is running.
    let a = new RedGreenCheck(fndecl, 0);
    if (a.hasRed)
      a.run();
  }
  
  functionPointerCheck(fndecl);
}

function RedGreenCheck(fndecl, trace) {
  //print("RedGreenCheck: " + fndecl.toCString());
  this._fndecl = fndecl;

  // Tell ESP that fndecl is a "property variable".  This makes ESP track it in
  // a flow-sensitive way.  The variable will be 1 in RED regions and "don't
  // know" in GREEN regions.  (We are technically lying to ESP about fndecl
  // being a variable--what we really want is a synthetic variable indicating
  // RED/GREEN state, but ESP operates on GCC decl nodes.)
  this._state_var_decl = fndecl;
  let state_var = new ESP.PropVarSpec(this._state_var_decl, true, undefined);

  // Call base class constructor.
  let cfg = function_decl_cfg(fndecl);
  ESP.Analysis.apply(this, [cfg, [state_var], Zero_NonZero.meet, trace]);
  this.join = Zero_NonZero.join;

  // Preprocess all instructions in the cfg to determine whether this analysis
  // is necessary and gather some information we'll use later.
  //
  // Each isn may include a function call, an assignment, and/or some reads.
  // Using walk_tree to walk the isns is a little crazy but robust.
  //
  this.hasRed = false;
  let self = this;         // Allow our 'this' to be accessed inside closure
  for (let bb in cfg_bb_iterator(cfg)) {
    for (let isn in bb_isn_iterator(bb)) {
      // Treehydra objects don't support reading never-defined properties
      // as undefined, so we have to explicitly initialize anything we want
      // to check for later.
      isn.redInfo = undefined;
      walk_tree(isn, function(t, stack) {
        function getLocation(skiptop) {
          if (!skiptop) {
            let loc = location_of(t);
            if (loc !== undefined)
              return loc;
          }
          
          for (let i = stack.length - 1; i >= 0; --i) {
            let loc = location_of(stack[i]);
            if (loc !== undefined)
              return loc;
          }
          return location_of(fndecl);
        }
                  
        switch (TREE_CODE(t)) {
          case FIELD_DECL:
            if (isRed(t)) {
              let varName = dehydra_convert(t).name;
              // location_of(t) is the location of the declaration.
              isn.redInfo = ["cannot access JS_REQUIRES_STACK variable " + varName,
                             getLocation(true)];
              self.hasRed = true;
            }
            break;
          case GIMPLE_CALL:
          {
            let callee = gimple_call_fndecl(t);
            if (callee) {
              if (isRed(callee)) {
                let calleeName = dehydra_convert(callee).name;
                isn.redInfo = ["cannot call JS_REQUIRES_STACK function " + calleeName,
                              getLocation(false)];
                self.hasRed = true;
              } else if (isTurnRed(callee)) {
                isn.turnRed = true;
              }
            }
            else {
              let fntype = TREE_CHECK(
                TREE_TYPE( // the function type
                  TREE_TYPE( // the function pointer type
                    gimple_call_fn(t)
                  )
                ),
                FUNCTION_TYPE, METHOD_TYPE);
              if (isRed(fntype)) {
                isn.redInfo = ["cannot call JS_REQUIRES_STACK function pointer",
                               getLocation(false)];
                self.hasRed = true;
              }
              else if (isTurnRed(fntype)) {
                isn.turnRed = true;
              }
            }
          }
          break;
        }
      });
    }
  }

  // Initialize mixin for infeasible-path elimination.
  this._zeroNonzero = new Zero_NonZero.Zero_NonZero();
}

RedGreenCheck.prototype = new ESP.Analysis;

RedGreenCheck.prototype.flowStateCond = function(isn, truth, state) {
  // forward event to mixin
  this._zeroNonzero.flowStateCond(isn, truth, state);
};

RedGreenCheck.prototype.flowState = function(isn, state) {
  // forward event to mixin
  //try { // The try/catch here is a workaround for some baffling bug in zero_nonzero.
    this._zeroNonzero.flowState(isn, state);
  //} catch (exc) {
  //  warning(exc, location_of(isn));
  //  warning("(Remove the workaround in jsstack.js and recompile to get a JS stack trace.)",
  //          location_of(isn));
  //}
  let stackState = state.get(this._state_var_decl);
  let green = stackState != 1 && stackState != ESP.NOT_REACHED;
  let redInfo = isn.redInfo;
  if (green && redInfo) {
    warning(redInfo[0], redInfo[1]);
    isn.redInfo = undefined;  // avoid duplicate messages about this instruction
  }

  // If we call a TURNS_RED function, it doesn't take effect until after the
  // whole isn finishes executing (the most conservative rule).
  if (isn.turnRed)
    state.assignValue(this._state_var_decl, 1, isn);
};

function followTypedefs(type)
{
  while (type.typedef !== undefined)
    type = type.typedef;
  return type;
}

function assignCheck(source, destType, locfunc)
{
  if (TREE_CODE(destType) != POINTER_TYPE)
    return;
    
  let destCode = TREE_CODE(TREE_TYPE(destType));
  if (destCode != FUNCTION_TYPE && destCode != METHOD_TYPE)
    return;
  
  if (isRed(TREE_TYPE(destType)))
    return;

  while (TREE_CODE(source) == NOP_EXPR)
    source = source.operands()[0];
  
  // The destination is a green function pointer

  if (TREE_CODE(source) == ADDR_EXPR) {
    let sourcefn = source.operands()[0];
    
    // oddly enough, SpiderMonkey assigns the address of something that's not
    // a function to a function pointer as part of the API! See JS_TN
    if (TREE_CODE(sourcefn) != FUNCTION_DECL)
      return;
    
    if (isRed(sourcefn))
      warning("Assigning non-JS_REQUIRES_STACK function pointer from JS_REQUIRES_STACK function " + dehydra_convert(sourcefn).name, locfunc());
  } else if (TREE_TYPE(source).tree_code() == POINTER_TYPE) {
    let sourceType = TREE_TYPE(TREE_TYPE(source));
    switch (TREE_CODE(sourceType)) {
      case FUNCTION_TYPE:
      case METHOD_TYPE:
        if (isRed(sourceType))
          warning("Assigning non-JS_REQUIRES_STACK function pointer from JS_REQUIRES_STACK function pointer", locfunc());
        break;
    }
  }
}

/**
 * A type checker which verifies that a red function pointer is never converted
 * to a green function pointer.
 */

function functionPointerWalk(t, baseloc)
{
  walk_tree(t, function(t, stack) {
    function getLocation(skiptop) {
      if (!skiptop) {
        let loc = location_of(t);
        if (loc !== undefined)
          return loc;
      }
          
      for (let i = stack.length - 1; i >= 0; --i) {
        let loc = location_of(stack[i]);
        if (loc !== undefined)
          return loc;
      }
      return location_of(baseloc);
    }
                  
    switch (TREE_CODE(t)) {
      case GIMPLE_ASSIGN: {
        let [dest, source] = t.operands();
        assignCheck(source, TREE_TYPE(dest), getLocation);
        break;
      }
      case CONSTRUCTOR: {
        let ttype = TREE_TYPE(t);
        switch (TREE_CODE(ttype)) {
          case RECORD_TYPE:
          case UNION_TYPE: {
            for each (let ce in VEC_iterate(CONSTRUCTOR_ELTS(t)))
              assignCheck(ce.value, TREE_TYPE(ce.index), getLocation);
            break;
          }
          case ARRAY_TYPE: {
            let eltype = TREE_TYPE(ttype);
            for each (let ce in VEC_iterate(CONSTRUCTOR_ELTS(t)))
              assignCheck(ce.value, eltype, getLocation);
            break;
          }
          case LANG_TYPE:
            // these can be safely ignored
            break;
          default:
            warning("Unexpected type in initializer: " + TREE_CODE(TREE_TYPE(t)), getLocation());
        }
        break;
      }
      case GIMPLE_CALL: {
        // Check that the arguments to a function and the declared types
        // of those arguments are compatible.
        let ops = t.operands();
        let funcType = TREE_TYPE( // function type
          TREE_TYPE(ops[1])); // function pointer type
        let argTypes = [t for (t in function_type_args(funcType))];
        for (let i = argTypes.length - 1; i >= 0; --i) {
          let destType = argTypes[i];
          let source = ops[i + 3];
          assignCheck(source, destType, getLocation);
        }
        break;
      }
    }
  });
}
  
function functionPointerCheck(fndecl)
{
  let cfg = function_decl_cfg(fndecl);
  for (let bb in cfg_bb_iterator(cfg))
    for (let isn in bb_isn_iterator(bb))
      functionPointerWalk(isn, fndecl);
}
