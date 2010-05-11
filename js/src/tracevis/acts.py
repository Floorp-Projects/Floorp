# Define JS engine activities

# This must match the C++ enum used by the engine instrumentation patch:
# For each int value of the enum, there should be a corresponding
# element in this list with an appropriate name.
states = [
    'exitlast',
    'interpret',
    'monitor',
    'record',
    'compile',
    'execute',
    'native',
]

# 'State' codes of this number or higher are actually 'instantaneous' events.
event_start = 8;

flush_reasons = [
    'B',
    'O',
    'S',
    'G'
]

# Must match C++ enum
reasons = [
    'none',
    'abort',
    'inner',
    'doubles',
    'callback',
    'anchor',
    'backoff',
    'cold',
    'record',
    'peers',
    'execute',
    'stabilize',
    'extendFlush',
    'extendMaxBranches',
    'extendStart',
    'extendCold',
    'scopeChainCheck',
    'extendOuter',
    'mismatchExit',
    'oomExit',
    'overflowExit',
    'timeoutExit',
    'deepBailExit',
    'statusExit',
    'otherExit',
    # Special stuff that doesn't match C++
    'start',
]

# Estimated speedup vs. the interpreter when doing a given activity.
# Activities not listed default to 0.
speedup_d = {
    'interpret': 1.0,
    'record': 0.95,
    'native': 2.5
}

speedups = [ speedup_d.get(name, 0) for name in states ]
