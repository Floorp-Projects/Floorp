function async() { return 42; } // overwritten by subsequent declaration

export default async // ASI occurs here due to the [no LineTerminator here] restriction on default-exporting an async function
function async() { return 17; }
