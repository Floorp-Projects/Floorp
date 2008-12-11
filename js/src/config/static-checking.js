/**
 * A script for GCC-dehydra to analyze the Mozilla codebase and catch
 * patterns that are incorrect, but which cannot be detected by a compiler. */

/**
 * Activate Treehydra outparams analysis if running in Treehydra.
 */

function treehydra_enabled() {
  return this.hasOwnProperty('TREE_CODE');
}

include('unstable/getopt.js');
[options, args] = getopt();

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
function process_function(f, stmts)
{
  for each (let module in modules)
    if (module.hasOwnProperty('process_function'))
      module.process_function(f, stmts);
}

function process_tree(fndecl)
{
  for each (let module in modules)
    if (module.hasOwnProperty('process_tree'))
      module.process_tree(fndecl);
}

function process_decl(decl)
{
  for each (let module in modules)
    if (module.hasOwnProperty('process_var'))
      module.process_decl(decl);
}

function process_cp_pre_genericize(fndecl)
{
  for each (let module in modules)
    if (module.hasOwnProperty('process_cp_pre_genericize'))
      module.process_cp_pre_genericize(fndecl);
}

function input_end()
{
  for each (let module in modules)
    if (module.hasOwnProperty('input_end'))
      module.input_end();
}
