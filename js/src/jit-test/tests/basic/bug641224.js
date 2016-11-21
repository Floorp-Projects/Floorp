// |jit-test| need-for-each

try {
x = evalcx('lazy');
x.__iterator__ = Object.isFrozen
for each(x in x) {}
} catch (e) {}
