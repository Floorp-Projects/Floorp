// |jit-test| need-for-each

try { eval("\
    function a(y){y.x}\
    for each(let d in[\
        ({}),({}),({}),({}),({}),({}),({}),({}),({}),({})\
    ]){\
        try{\
            a(d)\
        }catch(e){}\
    }\
    n\
") } catch (e) {}
