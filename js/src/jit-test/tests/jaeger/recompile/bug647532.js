try { Function("\
    __defineSetter__(\"x\",Object.keys)\
    (z=x instanceof[].some)\
")() } catch (e) { }
