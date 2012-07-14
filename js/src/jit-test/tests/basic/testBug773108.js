// |jit-test| error:InternalError

Function("\
for each(l in[(let(c)([])\
for each(l in[]))(let(c)w for(u in[]))(let(u)w for(l in[]))(let(c)w for(u in[]))\
(let(u)w for each(l in[]))(let(c)w for(u in[]))(let(u)w for(l in[]))(let(c)w for(u in[]))\
(let(l)w for(l in[]))(let(u)w for(l in['']))(let(c)w for(u in[]))(let(u)w for(l in[]))\
(let(c)w for(l in[]))(let(l)w for(l in[]))(let(c)w for(l in[]))(let(u)w for(l in[]))\
(let(c)w for(l in[]))(let(u)w for each(l in[x]))(let(w,x)w for(u in[]))]){}\
")
