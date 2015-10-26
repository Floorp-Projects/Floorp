// Test that static eval scopes don't mess with statement nested scope logic in
// the frontend.
eval("if (true) { { let y; } } else {}")
