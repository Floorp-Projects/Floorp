"""
From Andre Burgaud's Blog, from the CTypes Wiki:
http://www.burgaud.com/bring-colors-to-the-windows-console-with-python/

Colors text in console mode application (win32).
Uses ctypes and Win32 methods SetConsoleTextAttribute and
GetConsoleScreenBufferInfo.

$Id: color_console.py 534 2009-05-10 04:00:59Z andre $
"""

from ctypes import windll, Structure, c_short, c_ushort, byref

SHORT = c_short
WORD = c_ushort


class COORD(Structure):
    """struct in wincon.h."""
    _fields_ = [
        ("X", SHORT),
        ("Y", SHORT)]


class SMALL_RECT(Structure):
    """struct in wincon.h."""
    _fields_ = [
        ("Left", SHORT),
        ("Top", SHORT),
        ("Right", SHORT),
        ("Bottom", SHORT)]


class CONSOLE_SCREEN_BUFFER_INFO(Structure):
    """struct in wincon.h."""
    _fields_ = [
        ("dwSize", COORD),
        ("dwCursorPosition", COORD),
        ("wAttributes", WORD),
        ("srWindow", SMALL_RECT),
        ("dwMaximumWindowSize", COORD)]


# winbase.h
STD_INPUT_HANDLE = -10
STD_OUTPUT_HANDLE = -11
STD_ERROR_HANDLE = -12

# wincon.h
FOREGROUND_BLACK = 0x0000
FOREGROUND_BLUE = 0x0001
FOREGROUND_GREEN = 0x0002
FOREGROUND_CYAN = 0x0003
FOREGROUND_RED = 0x0004
FOREGROUND_MAGENTA = 0x0005
FOREGROUND_YELLOW = 0x0006
FOREGROUND_GREY = 0x0007
FOREGROUND_INTENSITY = 0x0008  # foreground color is intensified.

BACKGROUND_BLACK = 0x0000
BACKGROUND_BLUE = 0x0010
BACKGROUND_GREEN = 0x0020
BACKGROUND_CYAN = 0x0030
BACKGROUND_RED = 0x0040
BACKGROUND_MAGENTA = 0x0050
BACKGROUND_YELLOW = 0x0060
BACKGROUND_GREY = 0x0070
BACKGROUND_INTENSITY = 0x0080  # background color is intensified.

stdout_handle = windll.kernel32.GetStdHandle(STD_OUTPUT_HANDLE)
SetConsoleTextAttribute = windll.kernel32.SetConsoleTextAttribute
GetConsoleScreenBufferInfo = windll.kernel32.GetConsoleScreenBufferInfo


def get_text_attr():
    csbi = CONSOLE_SCREEN_BUFFER_INFO()
    GetConsoleScreenBufferInfo(stdout_handle, byref(csbi))
    return csbi.wAttributes


DEFAULT_COLORS = get_text_attr()


class Terminal(object):
    COLOR = {
        'black': 0x0000,
        'blue': 0x0001,
        'green': 0x0002,
        'cyan': 0x0003,
        'red': 0x0004,
        'magenta': 0x0005,
        'yellow': 0x0006,
        'gray': 0x0007
    }
    BRIGHT_INTENSITY = 0x0008
    BACKGROUND_SHIFT = 4

    @classmethod
    def set_color(cls, color):
        """
        color: str - color definition string
        """
        color_code = 0
        if color.startswith('bright'):
            color_code |= cls.BRIGHT_INTENSITY
            color = color[len('bright'):]
        color_code |= Terminal.COLOR[color]
        SetConsoleTextAttribute(stdout_handle, color_code)

    @classmethod
    def reset_color(cls):
        SetConsoleTextAttribute(stdout_handle, DEFAULT_COLORS)

    @classmethod
    def clear_right(cls):
        pass
