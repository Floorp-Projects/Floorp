import sys

class Terminal(object):
    COLOR = {
        'red': '31',
        'green': '32',
        'blue': '34',
        'gray': '37'
    }
    NORMAL_INTENSITY = '1'
    BRIGHT_INTENSITY = '2'
    ESCAPE = '\x1b['
    RESET = '0'
    SEPARATOR = ';'
    COLOR_CODE = 'm'
    CLEAR_RIGHT_CODE = 'K'

    @classmethod
    def set_color(cls, color):
        """
        color: str - color definition string
        """
        mod = Terminal.NORMAL_INTENSITY
        if color.startswith('bright'):
            mod = Terminal.BRIGHT_INTENSITY
            color = color[len('bright'):]
        color_code = Terminal.COLOR[color]

        sys.stdout.write(cls.ESCAPE + color_code + cls.SEPARATOR + mod + cls.COLOR_CODE)

    @classmethod
    def reset_color(cls):
        sys.stdout.write(cls.ESCAPE + cls.RESET + cls.COLOR_CODE)

    @classmethod
    def clear_right(cls):
        sys.stdout.write(cls.ESCAPE + cls.CLEAR_RIGHT_CODE)
