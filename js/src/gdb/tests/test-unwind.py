# Test the unwinder and the frame filter.
# flake8:  NOQA: F821
import platform


def do_unwinder_test():
    # The unwinder is disabled by default for the moment. Turn it on to check
    # that the unwinder works as expected.
    import gdb

    gdb.execute("enable unwinder .* SpiderMonkey")

    run_fragment("unwind.simple", "Something")

    first = True
    # The unwinder is a bit flaky still but should at least be able to
    # recognize one set of entry and exit frames.  This also tests to
    # make sure we didn't end up solely in the interpreter.
    found_entry = False
    found_exit = False
    found_main = False
    found_inner = False
    found_outer = False
    frames = list(gdb.frames.execute_frame_filters(gdb.newest_frame(), 0, -1))
    for frame in frames:
        print("examining " + frame.function())
        if first:
            assert_eq(frame.function().startswith("Something"), True)
            first = False
        elif frame.function() == "<<FrameType::Exit>>":
            found_exit = True
        elif frame.function() == "<<FrameType::CppToJSJit>>":
            found_entry = True
        elif frame.function() == "main":
            found_main = True
        elif "unwindFunctionInner" in frame.function():
            found_inner = True
        elif "unwindFunctionOuter" in frame.function():
            found_outer = True

    # Had to have found a frame.
    assert_eq(first, False)
    # Had to have found main.
    assert_eq(found_main, True)
    # Had to have found the entry and exit frames.
    assert_eq(found_exit, True)
    assert_eq(found_entry, True)
    # Had to have found the names of the two JS functions.
    assert_eq(found_inner, True)
    assert_eq(found_outer, True)


# Only on the right platforms.
if platform.machine() == "x86_64" and platform.system() == "Linux":
    # Only test when gdb has the unwinder feature.
    try:
        import gdb.unwinder  # NOQA: F401

        do_unwinder_test()
    except Exception:
        pass
