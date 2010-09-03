/**
 * A script for GCC-dehydra to analyze the Mozilla codebase and catch
 * patterns that are incorrect, but which cannot be detected by a compiler. */

/**
 * Activate Treehydra outparams analysis if running in Treehydra.
 */

function treehydra_enabled() {
  return this.hasOwnProperty('TREE_CODE');
}

sys.include_path.push(options.topsrcdir);

include('string-format.js');

let modules = [];

function LoadModules(modulelist)
{
  if (modulelist == "")
    return;

  let modulenames = modulelist.split(',');
  for each (let modulename in modulenames) {
    let module = { __proto__: this };
    include(modulename, module);
    modules.push(module);
  }
}

LoadModules(options['dehydra-modules']);
if (treehydra_enabled())
  LoadModules(options['treehydra-modules']);

function process_type(c)
{
  for each (let module in modules)
    if (module.hasOwnProperty('process_type'))
      module.process_type(c);
}

function hasAttribute(c, attrname)
{
  var attr;

  if (c.attributes === undefined)
    return false;

  for each (attr in c.attributes)
    if (attr.name == 'user' && attr.value[0] == attrname)
      return true;

  return false;
}

// This is useful for detecting method overrides
function signaturesMatch(m1, m2)
{
  if (m1.shortName != m2.shortName)
    return false;

  if ((!!m1.isVirtual) != (!!m2.isVirtual))
    return false;
  
  if (m1.isStatic != m2.isStatic)
    return false;
  
  let p1 = m1.type.parameters;
  let p2 = m2.type.parameters;

  if (p1.length != p2.length)
    return false;
  
  for (let i = 0; i < p1.length; ++i)
    if (!params_match(p1[i], p2[i]))
      return false;

  return true;
}

function params_match(p1, p2)
{
  [p1, p2] = unwrap_types(p1, p2);

  for (let i in p1)
    if (i == "type" && !types_match(p1.type, p2.type))
      return false;
    else if (i != "type" && p1[i] !== p2[i])
      return false;

  for (let i in p2)
    if (!(i in p1))
      return false;

  return true;
}

function types_match(t1, t2)
{
  if (!t1 || !t2)
    return false;

  [t1, t2] = unwrap_types(t1, t2);

  return t1 === t2;
}

function unwrap_types(t1, t2)
{
  while (t1.variantOf)
    t1 = t1.variantOf;

  while (t2.variantOf)
    t2 = t2.variantOf;

  return [t1, t2];
}

const forward_functions = [
  'process_type',
  'process_tree_type',
  'process_decl',
  'process_tree_decl',
  'process_function',
  'process_tree',
  'process_cp_pre_genericize',
  'input_end'
];

function setup_forwarding(n)
{
  this[n] = function() {
    for each (let module in modules) {
      if (module.hasOwnProperty(n)) {
        module[n].apply(this, arguments);
      }
    }
  }
}

for each (let n in forward_functions)
  setup_forwarding(n);
