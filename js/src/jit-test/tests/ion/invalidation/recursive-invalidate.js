var causeOSI = true;

function rec(x, self) {
    if (x === 0 || x !== x) {
        if (causeOSI) {
            causeOSI = false;
            self(NaN, self)
            causeOSI = true;
        }
        return;
    }
    self(x - 1, self);
}

// Use enough iterations to type infer the script.
causeOSI = false;
for (var i = 0; i < 20; ++i)
    rec(1, rec);
causeOSI = true;

rec(2, rec)
