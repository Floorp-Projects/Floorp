g = newGlobal();
Debugger(g).memory.trackingAllocationSites = true;
evaluate("function h() { 'use asm'; return {}}", { global: g });
