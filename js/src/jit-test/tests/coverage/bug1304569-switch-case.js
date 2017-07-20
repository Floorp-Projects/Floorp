
s = newGlobal()
evalcx("\
    switch (0) {\
        default: break;\
        case 1:\
                this.s += this.s;\
                g(h(\"\", 2));\
                break;\
                break\
    }\
", s)
evalcx("getLcovInfo()", s)
