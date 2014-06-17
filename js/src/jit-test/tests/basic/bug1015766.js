gczeal(8, 2)
try {
    [new String, y]
} catch (e) {}
r = /()/
try {
    "".replace(r, () => {
	[]()
    });
} catch(e) {}
