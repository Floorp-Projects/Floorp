var y = undefined;

try {} catch (x) {
    try {} catch (x) {
        try {} catch (x) {
        }
    }
}

try {} catch (x if y) {
    try {} catch (x if y) {
        try {} catch (x if y) {
        }
    }
}

while (false) {
    try {} catch ({x,y} if x) {
        try {} catch ({a,b,c,d} if a) {
            if (b) break;
            if (c) continue;
        }
    } finally {}
}

Label1:
for (let foo = 0; foo < 0; foo++) {
    Label2:
    for (let bar = 0; bar < 0; bar++) {
        if (foo) {
            if (bar)
                break Label2;
            continue Label2;
        } else {
            if (bar)
                break Label1;
            continue Label1;
        }
    }
}

Label3:
for (let foo = 0; foo < 0; foo++) {
    Label4:
    for (let bar = 0; bar < 0; bar++) {
        if (foo) {
            if (bar)
                continue Label4;
            break Label4;
        } else {
            if (bar)
                continue Label3;
            break Label3;
        }
    }
}

switch (42) {
default:
    try {} catch (x) {
        if (x + 1) {
            if (x)
                break;
            break;
        }
    }
    break;
}

try {
    null.x;
} catch (x) {
}
