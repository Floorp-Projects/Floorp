// The proxy is going to mutate thisv in place. InvokeSessionGuard should be
// cool with that
with(evalcx(''))[7, 8].map(Int16Array, [])
