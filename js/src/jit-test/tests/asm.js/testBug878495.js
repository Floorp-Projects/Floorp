var disassemble = disassemble || false;
if (disassemble)
{
    disassemble("-r", (function() {
        (function() {
            "use asm"
            return {}
        })()
    }))
}
