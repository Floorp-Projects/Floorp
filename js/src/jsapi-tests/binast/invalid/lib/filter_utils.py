# Utilities to modify JSON encoded AST
#
# All functions raise exception on unexpected case, to avoid overlooking AST
# change.

import copy


# Class to provide utility methods on JSON-encoded BinAST tree
#
# TODO: support enum, property key, float, bool, offset
class wrap:
    def __init__(self, obj):
        self.obj = obj

    # ==== tagged tuple ====

    def assert_tagged_tuple(self):
        """
        Assert that this object is a tagged tuple

        :return (wrap)
                self
        :raises Exception
                If this is not a tagged tuple
        """
        type_ = self.obj['@TYPE']
        if type_ != 'tagged tuple':
            raise Exception('expected a tagged tuple, got {}'.format(type_))
        return self

    def copy(self):
        """
        Deep copy a tagged tuple

        :return (wrap)
                The wrapped copied tagged tuple
        """
        self.assert_tagged_tuple()
        return wrap(copy.deepcopy(self.obj))

    # ==== interface ====

    def assert_interface(self, expected_name):
        """
        Assert that this object is a tagged tuple for given interface

        :param expected_name (string)
               The name of the interface
        :return (wrap)
                self
        :raises Exception
                If this is not an interface
        """
        self.assert_tagged_tuple()
        actual_name = self.obj['@INTERFACE']
        if actual_name != expected_name:
            raise Exception('expected interface {}, got {}'.format(
                expected_name, actual_name))
        return self

    def field(self, name):
        """
        Returns the field of the tagged tuple.

        :param name (string)
               The name of the field to get
        :return (wrap)
                The wrapped field value
        :raises Exception
                If there's no such field
        """
        self.assert_tagged_tuple()
        fields = self.obj['@FIELDS']
        for field in fields:
            if field['@FIELD_NAME'] == name:
                return wrap(field['@FIELD_VALUE'])
        raise Exception('No such field: {}'.format(name))

    def append_field(self, name, value):
        """
        Append a field to the tagged tuple

        :param name (string)
               the name of the field to add
        :param value (wrap)
               the wrapped value of the field to add
        :return (wrap)
               the wrapped value
        :raises Exception
                If name or value are not correct type
        """
        self.assert_tagged_tuple()
        if type(name) is not str and type(name) is not unicode:
            raise Exception('passed name is not string: {}'.format(name))
        if not isinstance(value, wrap):
            raise Exception('passed value is not wrap: {}'.format(value))
        fields = self.obj['@FIELDS']
        fields.append({
            '@FIELD_NAME': name,
            '@FIELD_VALUE': value.obj,
        })
        return value

    def replace_field(self, name, value):
        """
        Replaces the field of the tagged tuple.

        :param name (string)
               the name of the field to replace
        :param value (wrap)
               the wrapped value of the field
        :return (wrap)
                the wrapped value
        :raises Exception
                If there's no such field
        """
        self.assert_tagged_tuple()
        if not isinstance(value, wrap):
            raise Exception('passed value is not wrap: {}'.format(value))
        fields = self.obj['@FIELDS']
        for field in fields:
            if field['@FIELD_NAME'] == name:
                field['@FIELD_VALUE'] = value.obj
                return self
        raise Exception('No such field: {}'.format(name))

    def remove_field(self, name):
        """
        Removes the field from the tagged tuple

        :param name (string)
               the name of the field to remove
        :return (wrap)
                the wrapped removed field value
        :raises Exception
                If there's no such field
        """
        self.assert_tagged_tuple()
        i = 0
        fields = self.obj['@FIELDS']
        for field in fields:
            if field['@FIELD_NAME'] == name:
                del fields[i]
                return wrap(field['@FIELD_VALUE'])
            i += 1
        raise Exception('No such field: {}'.format(name))

    def set_interface_name(self, name):
        """
        Set the tagged tuple's interface name

        :param name (string)
               The name of the interface
        :return (wrap)
                self
        :raises Exception
                If name is not correct type
        """
        self.assert_tagged_tuple()
        self.obj['@INTERFACE'] = name
        if type(name) is not str and type(name) is not unicode:
            raise Exception('passed value is not string: {}'.format(name))
        return self

    # ==== list ====

    def assert_list(self):
        """
        Assert that this object is a list

        :return (wrap)
                self
        :raises Exception
                If this is not a list
        """
        type_ = self.obj['@TYPE']
        if type_ != 'list':
            raise Exception('expected a list, got {}'.format(type_))
        return self

    def elem(self, index):
        """
        Returns the element of the list.

        :param index (int)
               The index of the element to get
        :return (wrap)
                The wrapped element value
        :raises Exception
                On out of bound access
        """
        self.assert_list()
        elems = self.obj['@VALUE']
        if index >= len(elems):
            raise Exception('Out of bound: {} >= {}'.format(index, len(elems)))
        return wrap(elems[index])

    def append_elem(self, elem):
        """
        Appends the element to the list.

        :param elem (wrap)
               the wrapped value to be added to the list
        :return (wrap)
                The wrapped appended element
        """
        self.assert_list()
        if not isinstance(elem, wrap):
            raise Exception('passed value is not wrap: {}'.format(elem))
        elems = self.obj['@VALUE']
        elems.append(elem.obj)
        return elem

    def replace_elem(self, index, elem):
        """
        Replaces the element of the list.

        :param index (int)
               The index of the element to replace
        :param elem (wrap)
               the wrapped value of the element
        :return (wrap)
                The wrapped replaced element
        :raises Exception
                On out of bound access
        """
        self.assert_list()
        if not isinstance(elem, wrap):
            raise Exception('passed value is not wrap: {}'.format(elem))
        elems = self.obj['@VALUE']
        if index >= len(elems):
            raise Exception('Out of bound: {} >= {}'.format(index, len(elems)))
        elems[index] = elem.obj
        return elem

    def remove_elem(self, index):
        """
        Removes the element from the list

        :param index (int)
               The index of the element to remove
        :return (wrap)
                The wrapped removed element
        :raises Exception
                On out of bound access
        """
        self.assert_list()
        elems = self.obj['@VALUE']
        if index >= len(elems):
            raise Exception('Out of bound: {} >= {}'.format(index, len(elems)))
        elem = elems[index]
        del elems[index]
        return wrap(elem)

    # ==== string ====

    def assert_string(self):
        """
        Assert that this object is a string

        :return (wrap)
                self
        :raises Exception
                If this is not a string
        """
        type_ = self.obj['@TYPE']
        if type_ != 'string':
            raise Exception('expected a string, got {}'.format(type_))
        return self

    def set_string(self, value):
        """
        Set string value

        :param value (int)
               the value to set
        :return (wrap)
                self
        :raises Exception
                If value is not correct type
        """
        self.assert_string()
        if type(value) is not str and type(value) is not unicode:
            raise Exception('passed value is not string: {}'.format(value))
        self.obj['@VALUE'] = value
        return self

    # ==== identifier name ====

    def assert_identifier_name(self):
        """
        Assert that this object is a identifier name

        :return (wrap)
                self
        :raises Exception
                If this is not an identifier name
        """
        type_ = self.obj['@TYPE']
        if type_ != 'identifier name':
            raise Exception('expected a identifier name, got {}'.format(type_))
        return self

    def set_identifier_name(self, value):
        """
        Set identifier name value

        :param value (str)
               the value to set
        :return (wrap)
                self
        :raises Exception
                If value is not correct type
        """
        self.assert_identifier_name()
        if type(value) is not str and type(value) is not unicode:
            raise Exception('passed value is not string: {}'.format(value))
        self.obj['@VALUE'] = value
        return self

    def set_null_identifier_name(self):
        """
        Set identifier name value to null

        :return (wrap)
                self
        """
        self.assert_identifier_name()
        self.obj['@VALUE'] = None
        return self

    # ==== unsigned long ====

    def assert_unsigned_long(self):
        """
        Assert that this object is a unsigned long

        :return (wrap)
                self
        :raises Exception
                If this is not an unsigned long
        """
        type_ = self.obj['@TYPE']
        if type_ != 'unsigned long':
            raise Exception('expected a unsigned long, got {}'.format(type_))
        return self

    def set_unsigned_long(self, value):
        """
        Set unsigned long value

        :param value (int)
               the value to set
        :return (wrap)
                self
        :raises Exception
                If value is not correct type
        """
        self.assert_unsigned_long()
        if type(value) is not int:
            raise Exception('passed value is not int: {}'.format(value))
        self.obj['@VALUE'] = value
        return self
