define hookpost-run
    if ($sigaction)
        call free($sigaction)
        set $sigaction = 0
    end
end

catch signal SIGSEGV
commands
    if !$sigaction
        set $sigaction = malloc(sizeof(sigaction))
    end
    set $ignored = __sigaction(11, 0, $sigaction)
    set $handler = ((struct sigaction *)$sigaction)->__sigaction_handler.sa_handler
    if $handler == AsmJSFaultHandler
        continue
    end
end
