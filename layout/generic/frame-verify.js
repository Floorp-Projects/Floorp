/**
 * graph-frameclasses.js: a dehydra script to collect information about
 * the class hierarchy of frame types.
 */

function inheritsFrom(t, baseName)
{
  let name = t.name;
  if (name == baseName)
    return true;
  
  for each (let base in t.bases)
    if (inheritsFrom(base.type, baseName))
      return true;
    
  return false;
}  

let output = [];

function process_type(t)
{
  if ((t.kind == "class" || t.kind == "struct")) {
    if (!t.isIncomplete && inheritsFrom(t, 'nsIFrame')) {
      if (inheritsFrom(t, 'nsISupports'))
        warning("nsIFrame derivative %s inherits from nsISupports but is not refcounted.".format(t.name), t.loc);
      
      let nonFrameBases = [];
      
      output.push('CLASS-DEF: %s'.format(t.name));

      for each (let base in t.bases) {
        if (inheritsFrom(base.type, 'nsIFrame')) {
          output.push('%s -> %s;'.format(base.type.name, t.name));
        }
        else if (base.type.name != 'nsQueryFrame') {
          nonFrameBases.push(base.type.name);
        }
      }
      
      output.push('%s [label="%s%s"];'.format(t.name, t.name,
                                              ["\\n(%s)".format(b) for each (b in nonFrameBases)].join('')));
    }
  }
}

let frameIIDRE = /::kFrameIID$/;
let queryFrameRE = /^do_QueryFrame::operator/;

/* A list of class names T that have do_QueryFrame<T> used */
let needIDs = [];

/* A map of class names that have a kFrameIID declared */
let haveIDs = {};

// We match up needIDs with haveIDs at the end because static variables are
// not present in the .members array of a type

function process_tree_decl(d)
{
  d = dehydra_convert(d);
  
  if (d.name && frameIIDRE.exec(d.name)) {
    haveIDs[d.memberOf.name] = 1;
  }
}

function process_cp_pre_genericize(d)
{
  d = dehydra_convert(d);
  if (queryFrameRE.exec(d.name) && d.template === undefined) {
    let templtype = d.type.type.type;
    while (templtype.typedef !== undefined)
      templtype = templtype.typedef;
      
    needIDs.push([templtype.name, d.loc]);
  }
}

function input_end()
{
  for each (let [name, loc] in needIDs) {
    if (!haveIDs.hasOwnProperty(name)) {
      error("nsQueryFrame<%s> found, but %s::kFrameIID is not declared".format(name, name), loc);
    }
  }

  if (output.length > 0) {
    write_file(sys.aux_base_name + '.framedata', output.join('\n'));
  }
}
