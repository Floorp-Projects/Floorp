# Pretty-printers for InterpreterRegs.

import gdb
import mozilla.prettyprinters as prettyprinters

prettyprinters.clear_module_printers(__name__)

from mozilla.prettyprinters import pretty_printer

# Cache information about the Interpreter types for this objfile.
class InterpreterTypeCache(object):
    def __init__(self):
        self.tValue = gdb.lookup_type('JS::Value')
        self.tJSOp = gdb.lookup_type('JSOp')

@pretty_printer('js::InterpreterRegs')
class InterpreterRegs(object):
    def __init__(self, value, cache):
        self.value = value
        self.cache = cache
        if not cache.mod_Interpreter:
            cache.mod_Interpreter = InterpreterTypeCache()
        self.itc = cache.mod_Interpreter

    # There's basically no way to co-operate with 'set print pretty' (how would
    # you get the current level of indentation?), so we don't even bother
    # trying. No 'children', just 'to_string'.
    def to_string(self):
        fp_ = 'fp_ = {}'.format(self.value['fp_'])
        slots = (self.value['fp_'] + 1).cast(self.itc.tValue.pointer())
        sp = 'sp = fp_.slots() + {}'.format(self.value['sp'] - slots)
        pc = self.value['pc']
        try:
            opcode = pc.dereference().cast(self.itc.tJSOp)
        except:
            opcode = 'bad pc'
        pc = 'pc = {} ({})'.format(pc.cast(self.cache.void_ptr_t), opcode)
        return '{{ {}, {}, {} }}'.format(fp_, sp, pc)

